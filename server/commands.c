#include "commands.h"
#include "mailbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Behandelt SEND-Command (Pro Version: Sender kommt aus Session)
void handle_send(int client_socket, const Session *session) {
    char receiver[MAX_USERNAME + 2];
    char subject[MAX_SUBJECT + 2];
    char line[BUFFER_SIZE];
    
    // Sender kommt aus der Session (LDAP Username)
    const char *sender = session->username;
    
    // Lese Receiver
    if (readline(client_socket, receiver, sizeof(receiver)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    receiver[strcspn(receiver, "\n")] = 0;
    
    // Lese Subject
    if (readline(client_socket, subject, sizeof(subject)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    subject[strcspn(subject, "\n")] = 0;
    
    // Validiere Receiver Username
    if (!validate_username(receiver)) {
        // Konsumiere verbleibende Message-Zeilen bis ".\n"
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) {
                break;
            }
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Prüfe Subject-Länge
    if (strlen(subject) > MAX_SUBJECT) {
        // Konsumiere verbleibende Message-Zeilen bis ".\n"
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) {
                break;
            }
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Erwirbt Lock auf Receiver-Inbox um Race Conditions zu vermeiden
    // wenn mehrere Prozesse gleichzeitig in dieselbe Inbox schreiben
    int lock_fd = acquire_user_lock(receiver);
    if (lock_fd == -1) {
        // Konsumiere verbleibende Message-Zeilen bis ".\n"
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) {
                break;
            }
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Erstelle Inbox für Receiver falls nötig
    if (create_user_inbox(receiver) == -1) {
        release_user_lock(lock_fd);
        // Konsumiere Message
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) break;
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Hole nächste freie Message-Nummer
    int msg_num = get_next_message_number(receiver);
    
    // Erstelle Message-Datei
    char msg_file[MAX_PATH];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, receiver, msg_num);
    
    FILE *fp = fopen(msg_file, "w");
    if (fp == NULL) {
        perror("fopen");
        release_user_lock(lock_fd);
        // Konsumiere Message
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) break;
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Schreibe Message-Metadaten
    fprintf(fp, "Sender: %s\n", sender);
    fprintf(fp, "Receiver: %s\n", receiver);
    fprintf(fp, "Subject: %s\n", subject);
    fprintf(fp, "Message:\n");
    
    // Lese Message-Zeilen bis ".\n"
    while (1) {
        int len = readline(client_socket, line, sizeof(line));
        if (len <= 0) {
            fclose(fp);
            unlink(msg_file); // Lösche unvollständige Message
            release_user_lock(lock_fd);
            write(client_socket, "ERR\n", 4);
            return;
        }
        
        // Ende der Message bei ".\n"
        if (strcmp(line, ".\n") == 0) {
            break;
        }
        
        fprintf(fp, "%s", line);
    }
    
    fclose(fp);
    release_user_lock(lock_fd);
    
    write(client_socket, "OK\n", 3); // Erfolgreich
}

// Behandelt LIST-Command (Pro Version: Username kommt aus Session)
void handle_list(int client_socket, const Session *session) {
    // Username kommt aus der Session
    const char *username = session->username;
    
    // Zähle Messages durch Probieren der Nummern 1, 2, 3, ...
    char subjects[100][MAX_SUBJECT + 1];
    int count = 0;
    int num = 1;
    
    while (num <= 100 && count < 100) {
        char msg_file[MAX_PATH];
        snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, num);
        
        FILE *fp = fopen(msg_file, "r");
        if (fp != NULL) {
            char line[BUFFER_SIZE];
            // Überspringe Sender- und Receiver-Zeilen
            fgets(line, sizeof(line), fp);
            fgets(line, sizeof(line), fp);
            // Lese Subject-Zeile
            if (fgets(line, sizeof(line), fp) != NULL) {
                if (strncmp(line, "Subject: ", 9) == 0) {
                    strncpy(subjects[count], line + 9, MAX_SUBJECT);
                    subjects[count][strcspn(subjects[count], "\n")] = 0;
                    count++;
                }
            }
            fclose(fp);
        }
        num++;
    }
    
    // Sende Anzahl der Messages
    char response[32];
    snprintf(response, sizeof(response), "%d\n", count);
    write(client_socket, response, strlen(response));
    
    // Sende alle Subjects
    for (int i = 0; i < count; i++) {
        write(client_socket, subjects[i], strlen(subjects[i]));
        write(client_socket, "\n", 1);
    }
}

// Behandelt READ-Command (Pro Version: Username kommt aus Session)
void handle_read(int client_socket, const Session *session) {
    char msg_num_str[32];
    
    // Username kommt aus der Session
    const char *username = session->username;
    
    // Lese Message-Nummer
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Konvertiere Message-Nummer zu Integer
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Öffne Message-Datei
    char msg_file[MAX_PATH];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, msg_num);
    
    FILE *fp = fopen(msg_file, "r");
    if (fp == NULL) {
        write(client_socket, "ERR\n", 4); // Message existiert nicht
        return;
    }
    
    write(client_socket, "OK\n", 3); // Erfolgreich
    
    // Sende gesamten Message-Inhalt
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        write(client_socket, line, strlen(line));
    }
    
    fclose(fp);
}

// Behandelt DEL-Command (Pro Version: Username kommt aus Session)
void handle_del(int client_socket, const Session *session) {
    char msg_num_str[32];
    
    // Username kommt aus der Session
    const char *username = session->username;
    
    // Lese Message-Nummer
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Konvertiere Message-Nummer zu Integer
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Erwirbt Lock um Race Conditions mit SEND-Operationen zu vermeiden
    int lock_fd = acquire_user_lock(username);
    if (lock_fd == -1) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Lösche Message-Datei
    char msg_file[MAX_PATH];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, msg_num);
    
    if (unlink(msg_file) == -1) {
        release_user_lock(lock_fd);
        write(client_socket, "ERR\n", 4); // Löschen fehlgeschlagen
        return;
    }
    
    release_user_lock(lock_fd);
    
    write(client_socket, "OK\n", 3); // Erfolgreich gelöscht
}
