// Usage: ./twmailer-server <port> <mail-spool-directory>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80

// Globale Variable für das Mail-Spool-Verzeichnis
char mail_spool_dir[256];

// Liest eine Zeile vom Socket (bis \n)
// Rückgabe: Anzahl gelesener Zeichen, -1 bei Fehler, 0 wenn Connection geschlossen
int readline(int fd, char *buffer, int max_len) {
    int i = 0;
    char c;
    int n;
    
    // Lese Zeichen für Zeichen bis Newline oder max_len erreicht
    while (i < max_len - 1) {
        n = read(fd, &c, 1);
        if (n == 1) {
            buffer[i++] = c;
            if (c == '\n') {
                break; // Newline gefunden, fertig
            }
        } else if (n == 0) {
            if (i == 0) {
                return 0; // Connection wurde geschlossen
            }
            break;
        } else {
            return -1; // Fehler beim Lesen
        }
    }
    
    buffer[i] = '\0'; // Null-Terminierung
    return i;
}

// Validiert Username (max 8 Zeichen, nur a-z und 0-9)
int validate_username(const char *username) {
    int len = strlen(username);
    
    // Prüfe Länge
    if (len == 0 || len > MAX_USERNAME) {
        return 0;
    }
    
    // Prüfe jedes Zeichen: nur a-z und 0-9 erlaubt
    for (int i = 0; i < len; i++) {
        if (!((username[i] >= 'a' && username[i] <= 'z') ||
              (username[i] >= '0' && username[i] <= '9'))) {
            return 0; // Ungültiges Zeichen gefunden
        }
    }
    return 1;
}

// Erstellt Inbox-Verzeichnis für User falls nicht vorhanden
int create_user_inbox(const char *username) {
    char inbox_path[512];
    snprintf(inbox_path, sizeof(inbox_path), "%s/%s", mail_spool_dir, username);
    
    struct stat st = {0};
    // Prüfe ob Verzeichnis existiert
    if (stat(inbox_path, &st) == -1) {
        // Verzeichnis existiert nicht, erstelle es
        if (mkdir(inbox_path, 0700) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

// Ermittelt die nächste freie Message-Nummer für einen User
int get_next_message_number(const char *username) {
    char msg_file[512];
    int num = 1;
    
    // Suche erste freie Nummer durch Probieren
    while (num < 1000) {
        snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, num);
        
        // Prüfe ob Datei existiert
        FILE *fp = fopen(msg_file, "r");
        if (fp == NULL) {
            return num; // Diese Nummer ist frei
        }
        fclose(fp);
        num++;
    }
    
    return num; // Fallback
}

// Behandelt SEND-Command
void handle_send(int client_socket) {
    char sender[MAX_USERNAME + 2];
    char receiver[MAX_USERNAME + 2];
    char subject[MAX_SUBJECT + 2];
    char line[BUFFER_SIZE];
    
    // Lese Sender
    if (readline(client_socket, sender, sizeof(sender)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    sender[strcspn(sender, "\n")] = 0; // Entferne Newline
    
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
    
    // Validiere beide Usernames
    if (!validate_username(sender) || !validate_username(receiver)) {
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
    
    // Erstelle Inbox für Receiver falls nötig
    if (create_user_inbox(receiver) == -1) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Hole nächste freie Message-Nummer
    int msg_num = get_next_message_number(receiver);
    
    // Erstelle Message-Datei
    char msg_file[512];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, receiver, msg_num);
    
    FILE *fp = fopen(msg_file, "w");
    if (fp == NULL) {
        perror("fopen");
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
    write(client_socket, "OK\n", 3); // Erfolgreich
}

// Behandelt LIST-Command
void handle_list(int client_socket) {
    char username[MAX_USERNAME + 2];
    
    // Lese Username
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "0\n", 2);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Validiere Username
    if (!validate_username(username)) {
        write(client_socket, "0\n", 2);
        return;
    }
    
    // Zähle Messages durch Probieren der Nummern 1, 2, 3, ...
    char subjects[100][MAX_SUBJECT + 1];
    int count = 0;
    int num = 1;
    
    while (num <= 100 && count < 100) {
        char msg_file[512];
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

// Behandelt READ-Command
void handle_read(int client_socket) {
    char username[MAX_USERNAME + 2];
    char msg_num_str[32];
    
    // Lese Username
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Lese Message-Nummer
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Validiere Username
    if (!validate_username(username)) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Konvertiere Message-Nummer zu Integer
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Öffne Message-Datei
    char msg_file[512];
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

// Behandelt DEL-Command
void handle_del(int client_socket) {
    char username[MAX_USERNAME + 2];
    char msg_num_str[32];
    
    // Lese Username
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Lese Message-Nummer
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Validiere Username
    if (!validate_username(username)) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Konvertiere Message-Nummer zu Integer
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    // Lösche Message-Datei
    char msg_file[512];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, msg_num);
    
    if (unlink(msg_file) == -1) {
        write(client_socket, "ERR\n", 4); // Löschen fehlgeschlagen
        return;
    }
    
    write(client_socket, "OK\n", 3); // Erfolgreich gelöscht
}

// Behandelt Client-Verbindung
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    
    // Empfange Commands in einer Schleife
    while (1) {
        int len = readline(client_socket, buffer, sizeof(buffer));
        if (len <= 0) {
            break; // Connection geschlossen oder Fehler
        }
        
        buffer[strcspn(buffer, "\n")] = 0; // Entferne Newline
        
        printf("Received command: %s\n", buffer);
        
        // Verarbeite Command
        if (strcmp(buffer, "SEND") == 0) {
            handle_send(client_socket);
        } else if (strcmp(buffer, "LIST") == 0) {
            handle_list(client_socket);
        } else if (strcmp(buffer, "READ") == 0) {
            handle_read(client_socket);
        } else if (strcmp(buffer, "DEL") == 0) {
            handle_del(client_socket);
        } else if (strcmp(buffer, "QUIT") == 0) {
            printf("Client disconnected\n");
            break; // Beende Schleife
        } else {
            write(client_socket, "ERR\n", 4); // Unbekannter Command
        }
    }
}

int main(int argc, char *argv[]) {
    // Prüfe Argumente
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <mail-spool-directory>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Lese Port und Mail-Spool-Directory
    int port = atoi(argv[1]); 
    strncpy(mail_spool_dir, argv[2], sizeof(mail_spool_dir) - 1);
    mail_spool_dir[sizeof(mail_spool_dir) - 1] = '\0';
    
    // Erstelle Mail-Spool-Verzeichnis falls nicht vorhanden
    struct stat st = {0};
    if (stat(mail_spool_dir, &st) == -1) {
        if (mkdir(mail_spool_dir, 0700) == -1) { // 0700 = rwx für Owner
            perror("Failed to create mail spool directory");
            return EXIT_FAILURE;
        }
    }
    
    // Erstelle Socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0); // AF_INET = IPv4, SOCK_STREAM = TCP
    if (server_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    
    // Setze Socket-Option SO_REUSEADDR (erlaubt schnelles Neustarten)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    // Bind Socket an Port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Alle Interfaces
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    // Listen für eingehende Verbindungen
    if (listen(server_socket, 5) == -1) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    printf("TW-Mailer Server listening on port %d\n", port);
    printf("Mail spool directory: %s\n", mail_spool_dir);
    
    // Iterativer Server: Accept und Handle Clients nacheinander
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        // Accept Client-Verbindung
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("accept");
            continue; // Nächster Client
        }
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Behandle Client-Anfragen
        handle_client(client_socket);
        
        // Schließe Client-Verbindung
        close(client_socket);
        printf("Client connection closed\n");
    }
    
    // Server-Socket schließen (wird nie erreicht)
    close(server_socket);
    return EXIT_SUCCESS;
}
