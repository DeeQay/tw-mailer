// Usage: ./twmailer-server <port> <mail-spool-directory>
// Concurrent Server mit fork()
// Pro Version: LOGIN mit LDAP, Session-Management, Blacklist

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ldap.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80
#define MAX_LDAP_USERNAME 256
#define MAX_PASSWORD 256
#define MAX_PATH 1024  // Größerer Buffer für Dateipfade

// LDAP Configuration
#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
#define LDAP_SEARCH_BASE "dc=technikum-wien,dc=at"

// Blacklist Configuration
#define MAX_LOGIN_ATTEMPTS 3
#define BLACKLIST_DURATION 60  // Sekunden (1 Minute)
#define BLACKLIST_FILE "blacklist.dat"

// Globale Variable für das Mail-Spool-Verzeichnis
char mail_spool_dir[256];

// Session-Struktur für authentifizierten Benutzer
typedef struct {
    int authenticated;           // 1 wenn eingeloggt, 0 sonst
    char username[MAX_LDAP_USERNAME]; // LDAP Username (z.B. if23b001)
} Session;

// Blacklist-Eintrag Struktur
typedef struct {
    char ip[INET_ADDRSTRLEN];   // IP-Adresse
    int attempts;                // Anzahl fehlgeschlagener Versuche
    time_t block_until;          // Zeitpunkt bis wann gesperrt (0 = nicht gesperrt)
} BlacklistEntry;

// Signal Handler für SIGCHLD - verhindert Zombie-Prozesse
void sigchld_handler(int sig) {
    (void)sig; // braucht man 
    // Reape alle beendeten Kindprozesse
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Erwirbt ein Lock auf die User-Inbox (für Synchronisation zwischen Prozessen)
// Rückgabe: Lock file descriptor, -1 bei Fehler
int acquire_user_lock(const char *username) {
    char lock_file[MAX_PATH];
    snprintf(lock_file, sizeof(lock_file), "%s/.%s.lock", mail_spool_dir, username);
    
    int lock_fd = open(lock_file, O_CREAT | O_RDWR, 0600);
    if (lock_fd == -1) {
        perror("open lock file");
        return -1;
    }
    
    // Exklusives Lock (blockierend)
    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock");
        close(lock_fd);
        return -1;
    }
    
    return lock_fd;
}

// Gibt das Lock auf die User-Inbox frei
void release_user_lock(int lock_fd) {
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
}

// Erwirbt ein Lock auf die Blacklist-Datei
int acquire_blacklist_lock() {
    char lock_file[MAX_PATH];
    snprintf(lock_file, sizeof(lock_file), "%s/.blacklist.lock", mail_spool_dir);
    
    int lock_fd = open(lock_file, O_CREAT | O_RDWR, 0600);
    if (lock_fd == -1) {
        perror("open blacklist lock file");
        return -1;
    }
    
    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock blacklist");
        close(lock_fd);
        return -1;
    }
    
    return lock_fd;
}

// Gibt das Blacklist-Lock frei
void release_blacklist_lock(int lock_fd) {
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
}

// Lädt Blacklist-Eintrag für eine IP
// Rückgabe: 1 wenn gefunden, 0 wenn nicht gefunden
int load_blacklist_entry(const char *ip, BlacklistEntry *entry) {
    char blacklist_path[MAX_PATH];
    snprintf(blacklist_path, sizeof(blacklist_path), "%s/%s", mail_spool_dir, BLACKLIST_FILE);
    
    FILE *fp = fopen(blacklist_path, "r");
    if (fp == NULL) {
        return 0; // Datei existiert nicht
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char stored_ip[INET_ADDRSTRLEN];
        int attempts;
        long block_until;
        
        if (sscanf(line, "%15s %d %ld", stored_ip, &attempts, &block_until) == 3) {
            if (strcmp(stored_ip, ip) == 0) {
                strncpy(entry->ip, stored_ip, INET_ADDRSTRLEN - 1);
                entry->ip[INET_ADDRSTRLEN - 1] = '\0';
                entry->attempts = attempts;
                entry->block_until = (time_t)block_until;
                fclose(fp);
                return 1;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

// Speichert/Aktualisiert Blacklist-Eintrag für eine IP
void save_blacklist_entry(const char *ip, int attempts, time_t block_until) {
    char blacklist_path[MAX_PATH];
    char temp_path[MAX_PATH];
    snprintf(blacklist_path, sizeof(blacklist_path), "%s/%s", mail_spool_dir, BLACKLIST_FILE);
    snprintf(temp_path, sizeof(temp_path), "%s/%s.tmp", mail_spool_dir, BLACKLIST_FILE);
    
    FILE *fp_in = fopen(blacklist_path, "r");
    FILE *fp_out = fopen(temp_path, "w");
    
    if (fp_out == NULL) {
        if (fp_in) fclose(fp_in);
        return;
    }
    
    int found = 0;
    
    if (fp_in != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp_in) != NULL) {
            char stored_ip[INET_ADDRSTRLEN];
            int stored_attempts;
            long stored_block;
            
            if (sscanf(line, "%15s %d %ld", stored_ip, &stored_attempts, &stored_block) == 3) {
                if (strcmp(stored_ip, ip) == 0) {
                    // Aktualisiere diesen Eintrag
                    fprintf(fp_out, "%s %d %ld\n", ip, attempts, (long)block_until);
                    found = 1;
                } else {
                    // Kopiere unverändert
                    fprintf(fp_out, "%s", line);
                }
            }
        }
        fclose(fp_in);
    }
    
    // Wenn IP nicht gefunden, füge neuen Eintrag hinzu
    if (!found) {
        fprintf(fp_out, "%s %d %ld\n", ip, attempts, (long)block_until);
    }
    
    fclose(fp_out);
    
    // Ersetze alte Datei durch neue
    rename(temp_path, blacklist_path);
}

// Prüft ob IP aktuell gesperrt ist
// Rückgabe: Verbleibende Sekunden wenn gesperrt, 0 wenn nicht gesperrt
int get_blacklist_remaining(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return 0;
    
    BlacklistEntry entry;
    int remaining = 0;
    
    if (load_blacklist_entry(ip, &entry)) {
        time_t now = time(NULL);
        if (entry.block_until > now) {
            remaining = (int)(entry.block_until - now);
        }
    }
    
    release_blacklist_lock(lock_fd);
    return remaining;
}

// Registriert fehlgeschlagenen Login-Versuch
// Rückgabe: Verbleibende Versuche (0 = jetzt gesperrt)
int register_failed_login(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return MAX_LOGIN_ATTEMPTS;
    
    BlacklistEntry entry;
    int attempts = 1;
    time_t block_until = 0;
    
    if (load_blacklist_entry(ip, &entry)) {
        time_t now = time(NULL);
        
        // Wenn Sperre abgelaufen, reset Attempts
        if (entry.block_until > 0 && entry.block_until <= now) {
            attempts = 1;
        } else {
            attempts = entry.attempts + 1;
        }
    }
    
    // Bei 3 Versuchen: Sperre für 1 Minute
    if (attempts >= MAX_LOGIN_ATTEMPTS) {
        block_until = time(NULL) + BLACKLIST_DURATION;
        printf("IP %s blocked for %d seconds\n", ip, BLACKLIST_DURATION);
    }
    
    save_blacklist_entry(ip, attempts, block_until);
    release_blacklist_lock(lock_fd);
    
    return MAX_LOGIN_ATTEMPTS - attempts;
}

// Registriert erfolgreichen Login (löscht Blacklist-Eintrag)
void register_successful_login(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return;
    
    save_blacklist_entry(ip, 0, 0); // Eintrag löschen
    release_blacklist_lock(lock_fd);
}

// Authentifiziert Benutzer gegen LDAP-Server
// Rückgabe: 1 bei Erfolg, 0 bei Auth-Fehler, -1 bei Server nicht erreichbar
int ldap_authenticate(const char *username, const char *password) {
    LDAP *ld = NULL;
    int result = 0;
    int ldap_version = LDAP_VERSION3;
    
    // Erstelle LDAP URI
    char ldap_uri[256];
    snprintf(ldap_uri, sizeof(ldap_uri), "ldap://%s:%d", LDAP_HOST, LDAP_PORT);
    
    // Initialisiere LDAP-Verbindung
    int rc = ldap_initialize(&ld, ldap_uri);
    if (rc != LDAP_SUCCESS) {
        fprintf(stderr, "ldap_initialize failed: %s\n", ldap_err2string(rc));
        return -1;
    }
    
    // LDAP Version 3
    rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldap_version);
    if (rc != LDAP_OPT_SUCCESS) {
        fprintf(stderr, "ldap_set_option failed: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return -1;
    }
    
    // Netzwerk-Timeout (5 Sekunden)
    struct timeval network_timeout;
    network_timeout.tv_sec = 5;
    network_timeout.tv_usec = 0;
    rc = ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &network_timeout);
    if (rc != LDAP_OPT_SUCCESS) {
        fprintf(stderr, "ldap_set_option (timeout) failed: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return -1;
    }
    
    // Erstelle DN für Benutzer: uid=<username>,ou=people,dc=technikum-wien,dc=at
    char bind_dn[MAX_PATH];
    snprintf(bind_dn, sizeof(bind_dn), "uid=%s,ou=people,%s", username, LDAP_SEARCH_BASE);
    
    // Erstelle Credentials
    struct berval cred;
    cred.bv_val = (char *)password;
    cred.bv_len = strlen(password);
    
    // Versuche LDAP bind (Authentifizierung)
    rc = ldap_sasl_bind_s(ld, bind_dn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
    
    if (rc == LDAP_SUCCESS) {
        result = 1;
        printf("LDAP authentication successful for user: %s\n", username);
    } else if (rc == LDAP_SERVER_DOWN || rc == LDAP_TIMEOUT || rc == LDAP_CONNECT_ERROR) {
        fprintf(stderr, "LDAP server unreachable: %s\n", ldap_err2string(rc));
        result = -1;
    } else {
        fprintf(stderr, "LDAP authentication failed for user %s: %s\n", 
                username, ldap_err2string(rc));
        result = 0;
    }
    
    // Schließe LDAP-Verbindung
    ldap_unbind_ext_s(ld, NULL, NULL);
    return result;
}

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
    char inbox_path[MAX_PATH];
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
    char msg_file[MAX_PATH];
    int num = 1;
    
    // Suche erste freie Nummer
    while (num < 1000) {
        snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", mail_spool_dir, username, num);
        if (access(msg_file, F_OK) == -1) {
            return num; // Nummer ist frei
        }
        num++;
    }
    
    return num;
}

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
    
    // ========== KRITISCHE SEKTION BEGIN ==========
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
    // ========== KRITISCHE SEKTION END ==========
    
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
    
    // ========== KRITISCHE SEKTION BEGIN ==========
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
    // ========== KRITISCHE SEKTION END ==========
    
    write(client_socket, "OK\n", 3); // Erfolgreich gelöscht
}

// Behandelt LOGIN-Command
void handle_login(int client_socket, Session *session, const char *client_ip) {
    char username[MAX_LDAP_USERNAME + 2];
    char password[MAX_PASSWORD + 2];
    
    // Lese LDAP Username
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Lese Password
    if (readline(client_socket, password, sizeof(password)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    password[strcspn(password, "\n")] = 0;
    
    // Prüfe ob IP gesperrt ist
    int blocked_seconds = get_blacklist_remaining(client_ip);
    if (blocked_seconds > 0) {
        char blocked_msg[32];
        printf("Login attempt from blacklisted IP: %s (%d seconds remaining)\n", client_ip, blocked_seconds);
        snprintf(blocked_msg, sizeof(blocked_msg), "BLOCKED %d\n", blocked_seconds);
        write(client_socket, blocked_msg, strlen(blocked_msg));
        return;
    }
    
    // Prüfe ob User bereits eingeloggt ist
    if (session->authenticated) {
        printf("User already logged in: %s\n", session->username);
        write(client_socket, "OK\n", 3);
        return;
    }
    
    // Authentifiziere gegen LDAP
    int auth_result = ldap_authenticate(username, password);
    if (auth_result == 1) {
        // Erfolgreicher Login
        session->authenticated = 1;
        strncpy(session->username, username, MAX_LDAP_USERNAME - 1);
        session->username[MAX_LDAP_USERNAME - 1] = '\0';
        
        // Reset Blacklist für diese IP
        register_successful_login(client_ip);
        
        printf("User %s logged in successfully from %s\n", username, client_ip);
        write(client_socket, "OK\n", 3);
    } else if (auth_result == -1) {
        // LDAP Server nicht erreichbar - kein Blacklist-Eintrag
        printf("LDAP server unreachable for user %s from %s\n", username, client_ip);
        write(client_socket, "ERR LDAP\n", 9);
    } else {
        // Fehlgeschlagener Login
        int remaining = register_failed_login(client_ip);
        printf("Login failed for user %s from %s (%d attempts remaining)\n", 
               username, client_ip, remaining);
        write(client_socket, "ERR\n", 4);
    }
}

// Behandelt Client-Verbindung (Pro Version mit Session-Management)
void handle_client(int client_socket, const char *client_ip) {
    char buffer[BUFFER_SIZE];
    
    // Initialisiere Session
    Session session;
    session.authenticated = 0;
    session.username[0] = '\0';
    
    // Empfange Commands in einer Schleife
    while (1) {
        int len = readline(client_socket, buffer, sizeof(buffer));
        if (len <= 0) {
            break; // Connection geschlossen oder Fehler
        }
        
        buffer[strcspn(buffer, "\n")] = 0; // Entferne Newline
        
        printf("Received command: %s\n", buffer);
        
        // QUIT ist immer erlaubt (auch ohne Login)
        if (strcmp(buffer, "QUIT") == 0) {
            printf("Client disconnected\n");
            break;
        }
        
        // LOGIN ist immer erlaubt
        if (strcmp(buffer, "LOGIN") == 0) {
            handle_login(client_socket, &session, client_ip);
            continue;
        }
        
        // Alle anderen Commands benötigen Authentifizierung
        if (!session.authenticated) {
            printf("Command %s rejected: not authenticated\n", buffer);
            write(client_socket, "ERR\n", 4);
            continue;
        }
        
        // Verarbeite Command (nur für authentifizierte User)
        if (strcmp(buffer, "SEND") == 0) {
            handle_send(client_socket, &session);
        } else if (strcmp(buffer, "LIST") == 0) {
            handle_list(client_socket, &session);
        } else if (strcmp(buffer, "READ") == 0) {
            handle_read(client_socket, &session);
        } else if (strcmp(buffer, "DEL") == 0) {
            handle_del(client_socket, &session);
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
    
    // Registriere Signal Handler für SIGCHLD um Zombie-Prozesse zu vermeiden
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
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
    
    printf("TW-Mailer Server (concurrent) listening on port %d\n", port);
    printf("Mail spool directory: %s\n", mail_spool_dir);
    
    // Concurrent Server mit fork(): Handle Clients parallel
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        // Accept Client-Verbindung
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            // EINTR kann durch SIGCHLD verursacht werden - ignorieren
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue; // Nächster Client
        }
        
        // Extrahiere Client-IP für Blacklist
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        printf("Client connected from %s:%d\n", 
               client_ip, 
               ntohs(client_addr.sin_port));
        
        // Fork einen Kindprozess für den Client
        pid_t pid = fork();
        
        if (pid == -1) {
            // Fork fehlgeschlagen
            perror("fork");
            close(client_socket);
            continue;
        } else if (pid == 0) {
            // ========== KINDPROZESS ==========
            // Kindprozess braucht den Server-Socket nicht
            close(server_socket);
            
            // Behandle Client-Anfragen (mit Client-IP für Blacklist)
            handle_client(client_socket, client_ip);
            
            // Schließe Client-Verbindung
            close(client_socket);
            printf("Child process %d: Client connection closed\n", getpid());
            
            // Kindprozess beenden
            exit(EXIT_SUCCESS);
        } else {
            // ========== ELTERNPROZESS ==========
            // Elternprozess braucht den Client-Socket nicht
            // Der Kindprozess hat eine Kopie davon
            close(client_socket);
            
            printf("Spawned child process %d for client\n", pid);
            // Weiter mit accept() für nächsten Client
        }
    }
    
    // Server-Socket schließen (wird nie erreicht)
    close(server_socket);
    return EXIT_SUCCESS;
}
