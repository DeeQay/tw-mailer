/*
 * ============================================================================
 * TW-MAILER SERVER - AUSFÜHRLICH KOMMENTIERTE VERSION (TEIL 1/2)
 * ============================================================================
 * 
 * ZWECK:
 * Diese Datei ist eine kommentierte Version des TW-Mailer Servers.
 * Sie dient dem besseren Verständnis der Funktionsweise.
 * 
 * VERWENDUNG:
 * ./twmailer-server <port> <mail-spool-verzeichnis>
 * 
 * BEISPIEL:
 * ./twmailer-server 6543 /tmp/mailspool
 * 
 * FUNKTIONSWEISE:
 * Der Server wartet auf TCP-Verbindungen von Clients und verarbeitet
 * deren Befehle (SEND, LIST, READ, DEL, QUIT). Nachrichten werden
 * als Dateien im Mail-Spool-Verzeichnis gespeichert.
 * 
 * VERZEICHNISSTRUKTUR:
 * <mail-spool>/
 *   └─ <username>/
 *       ├─ 1         (Nachricht 1)
 *       ├─ 2         (Nachricht 2)
 *       └─ ...
 * ============================================================================
 */

#include <stdio.h>      // Standard Input/Output
#include <stdlib.h>     // Standard Library (exit, atoi, etc.)
#include <string.h>     // String-Funktionen
#include <unistd.h>     // UNIX Standard (read, write, close)
#include <sys/socket.h> // Socket-Programmierung
#include <netinet/in.h> // Internet-Adressen
#include <arpa/inet.h>  // IP-Adressen-Konvertierung
#include <sys/stat.h>   // Datei-/Verzeichnis-Status (mkdir, stat)

/* ============================================================================
 * KONSTANTEN
 * ============================================================================ */

#define BUFFER_SIZE 1024   // Maximale Buffer-Größe
#define MAX_USERNAME 8     // Maximale Username-Länge
#define MAX_SUBJECT 80     // Maximale Betreff-Länge

/* ============================================================================
 * GLOBALE VARIABLEN
 * ============================================================================ */

// Pfad zum Mail-Spool-Verzeichnis (wo alle Nachrichten gespeichert werden)
// WARUM GLOBAL? Wird in vielen Funktionen benötigt
char mail_spool_dir[256];

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

/**
 * FUNKTION: readline
 * ------------------
 * Identisch zur Client-Version - siehe dort für Details.
 * Liest eine Zeile vom Socket bis zum Newline-Zeichen.
 */
int readline(int fd, char *buffer, int max_len) {
    int i = 0;
    char c;
    int n;
    
    while (i < max_len - 1) {
        n = read(fd, &c, 1);
        if (n == 1) {
            buffer[i++] = c;
            if (c == '\n') {
                break;
            }
        } else if (n == 0) {
            if (i == 0) {
                return 0;
            }
            break;
        } else {
            return -1;
        }
    }
    
    buffer[i] = '\0';
    return i;
}

/**
 * FUNKTION: validate_username
 * ---------------------------
 * Prüft ob ein Username den Protokoll-Anforderungen entspricht.
 * 
 * ANFORDERUNGEN:
 * - Länge: 1 bis MAX_USERNAME (8) Zeichen
 * - Erlaubte Zeichen: a-z (Kleinbuchstaben) und 0-9 (Ziffern)
 * - Keine Sonderzeichen, Leerzeichen oder Großbuchstaben
 * 
 * WARUM WICHTIG?
 * - Sicherheit: Verhindert Path-Traversal-Angriffe (z.B. "../etc/passwd")
 * - Konsistenz: Alle Usernames folgen demselben Format
 * 
 * PARAMETER:
 *   username - Zu prüfender Username
 * 
 * RÜCKGABE:
 *   1 = gültig
 *   0 = ungültig
 */
int validate_username(const char *username) {
    int len = strlen(username);
    
    /* ------ SCHRITT 1: Länge prüfen ------ */
    if (len == 0 || len > MAX_USERNAME) {
        return 0;  // Zu kurz oder zu lang
    }
    
    /* ------ SCHRITT 2: Jedes Zeichen prüfen ------ */
    for (int i = 0; i < len; i++) {
        // Prüfe ob Zeichen im erlaubten Bereich liegt
        // a-z: ASCII 97-122
        // 0-9: ASCII 48-57
        if (!((username[i] >= 'a' && username[i] <= 'z') ||
              (username[i] >= '0' && username[i] <= '9'))) {
            return 0;  // Ungültiges Zeichen gefunden
        }
    }
    
    return 1;  // Alle Prüfungen bestanden
}

/**
 * FUNKTION: create_user_inbox
 * ---------------------------
 * Erstellt das Inbox-Verzeichnis für einen Benutzer, falls es noch nicht existiert.
 * 
 * BEISPIEL:
 * Wenn mail_spool_dir="/tmp/mailspool" und username="alice",
 * wird "/tmp/mailspool/alice" erstellt.
 * 
 * PARAMETER:
 *   username - Name des Benutzers
 * 
 * RÜCKGABE:
 *   0  = Erfolg (Verzeichnis existiert oder wurde erstellt)
 *   -1 = Fehler beim Erstellen
 */
int create_user_inbox(const char *username) {
    char inbox_path[512];
    
    // snprintf() = sicheres Formatieren eines Strings
    // Erstellt Pfad: "<mail_spool_dir>/<username>"
    snprintf(inbox_path, sizeof(inbox_path), "%s/%s", mail_spool_dir, username);
    
    // struct stat wird verwendet um Datei-/Verzeichnis-Informationen zu speichern
    struct stat st = {0};
    
    /* ------ Prüfe ob Verzeichnis bereits existiert ------ */
    // stat() gibt 0 zurück wenn Pfad existiert, -1 wenn nicht
    if (stat(inbox_path, &st) == -1) {
        // Verzeichnis existiert nicht → erstelle es
        
        // mkdir() erstellt ein neues Verzeichnis
        // 0700 = Berechtigungen: rwx------ (nur Owner kann lesen/schreiben/ausführen)
        if (mkdir(inbox_path, 0700) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    
    return 0;  // Erfolg
}

/**
 * FUNKTION: get_next_message_number
 * ---------------------------------
 * Ermittelt die nächste freie Nachrichtennummer für einen Benutzer.
 * 
 * FUNKTIONSWEISE:
 * Probiert Nummern 1, 2, 3, ... durch, bis eine freie gefunden wird.
 * Eine Nummer ist frei, wenn keine Datei mit diesem Namen existiert.
 * 
 * WARUM SO?
 * Einfache Implementierung ohne Datenbank. Wenn Nachricht 2 gelöscht wird,
 * kann diese Nummer wiederverwendet werden.
 * 
 * PARAMETER:
 *   username - Name des Benutzers
 * 
 * RÜCKGABE:
 *   Nächste freie Nachrichtennummer (1-999)
 */
int get_next_message_number(const char *username) {
    char msg_file[512];
    int num = 1;  // Beginne bei 1
    
    // Probiere Nummern durch bis 1000 (Limit)
    while (num < 1000) {
        // Erstelle Dateipfad für diese Nummer
        snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
                 mail_spool_dir, username, num);
        
        // Versuche Datei zu öffnen
        FILE *fp = fopen(msg_file, "r");
        
        if (fp == NULL) {
            // Datei existiert nicht → diese Nummer ist frei!
            return num;
        }
        
        // Datei existiert → schließen und nächste Nummer probieren
        fclose(fp);
        num++;
    }
    
    // Fallback (sollte nie erreicht werden bei normalem Gebrauch)
    return num;
}

/* ============================================================================
 * PROTOKOLL-HANDLER (BEFEHLE)
 * ============================================================================ */

/**
 * FUNKTION: handle_send
 * ---------------------
 * Verarbeitet den SEND-Befehl vom Client.
 * 
 * PROTOKOLL (was der Server vom Client erwartet):
 * 1. "SEND\n" (bereits empfangen)
 * 2. "<sender>\n"
 * 3. "<receiver>\n"
 * 4. "<subject>\n"
 * 5. "<nachricht-zeilen>\n...\n.\n"
 * 
 * ABLAUF:
 * 1. Empfange alle Daten
 * 2. Validiere Sender, Receiver und Subject
 * 3. Erstelle Inbox für Receiver
 * 4. Speichere Nachricht als Datei
 * 5. Sende "OK\n" oder "ERR\n"
 * 
 * PARAMETER:
 *   client_socket - Socket des Clients
 */
void handle_send(int client_socket) {
    // Buffer für empfangene Daten
    char sender[MAX_USERNAME + 2];    // +2 für \n und \0
    char receiver[MAX_USERNAME + 2];
    char subject[MAX_SUBJECT + 2];
    char line[BUFFER_SIZE];
    
    /* ------ SCHRITT 1: Empfange Sender ------ */
    if (readline(client_socket, sender, sizeof(sender)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    sender[strcspn(sender, "\n")] = 0;  // Entferne Newline
    
    /* ------ SCHRITT 2: Empfange Receiver ------ */
    if (readline(client_socket, receiver, sizeof(receiver)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    receiver[strcspn(receiver, "\n")] = 0;
    
    /* ------ SCHRITT 3: Empfange Subject ------ */
    if (readline(client_socket, subject, sizeof(subject)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    subject[strcspn(subject, "\n")] = 0;
    
    /* ------ SCHRITT 4: Validiere Usernames ------ */
    if (!validate_username(sender) || !validate_username(receiver)) {
        // WICHTIG: Konsumiere noch die Message-Zeilen bis ".\n"
        // Sonst ist das Protokoll nicht mehr synchron!
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) {
                break;
            }
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 5: Validiere Subject-Länge ------ */
    if (strlen(subject) > MAX_SUBJECT) {
        // Konsumiere auch hier die restlichen Zeilen
        while (1) {
            int len = readline(client_socket, line, sizeof(line));
            if (len <= 0 || strcmp(line, ".\n") == 0) {
                break;
            }
        }
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 6: Erstelle Inbox für Empfänger ------ */
    if (create_user_inbox(receiver) == -1) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 7: Ermittle Nachrichtennummer ------ */
    int msg_num = get_next_message_number(receiver);
    
    /* ------ SCHRITT 8: Erstelle Nachrichtendatei ------ */
    char msg_file[512];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
             mail_spool_dir, receiver, msg_num);
    
    // Öffne Datei zum Schreiben ("w" = write mode)
    FILE *fp = fopen(msg_file, "w");
    if (fp == NULL) {
        perror("fopen");
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 9: Schreibe Nachricht in Datei ------ */
    // fprintf() = printf() in eine Datei
    fprintf(fp, "Sender: %s\n", sender);
    fprintf(fp, "Receiver: %s\n", receiver);
    fprintf(fp, "Subject: %s\n", subject);
    fprintf(fp, "Message:\n");
    
    /* ------ SCHRITT 10: Empfange und speichere Nachrichtentext ------ */
    while (1) {
        int len = readline(client_socket, line, sizeof(line));
        
        if (len <= 0) {
            // Fehler beim Empfangen → Datei löschen (unvollständig)
            fclose(fp);
            unlink(msg_file);  // unlink() = Datei löschen
            write(client_socket, "ERR\n", 4);
            return;
        }
        
        // Prüfe auf Ende-Marker
        if (strcmp(line, ".\n") == 0) {
            break;  // Nachricht vollständig
        }
        
        // Schreibe Zeile in Datei
        fprintf(fp, "%s", line);
    }
    
    /* ------ SCHRITT 11: Erfolgreich abschließen ------ */
    fclose(fp);  // Datei schließen (wichtig!)
    write(client_socket, "OK\n", 3);  // Erfolg an Client melden
}

/**
 * FUNKTION: handle_list
 * ---------------------
 * Verarbeitet den LIST-Befehl vom Client.
 * 
 * PROTOKOLL:
 * 1. "LIST\n" (bereits empfangen)
 * 2. "<username>\n"
 * → Server antwortet:
 * 3. "<anzahl>\n"
 * 4. "<subject1>\n<subject2>\n..." (anzahl Zeilen)
 * 
 * ABLAUF:
 * 1. Empfange Username
 * 2. Durchsuche Inbox nach Nachrichten (1, 2, 3, ...)
 * 3. Extrahiere Subject aus jeder Nachricht
 * 4. Sende Anzahl und alle Subjects
 * 
 * PARAMETER:
 *   client_socket - Socket des Clients
 */
void handle_list(int client_socket) {
    char username[MAX_USERNAME + 2];
    
    /* ------ SCHRITT 1: Empfange Username ------ */
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "0\n", 2);  // 0 Nachrichten bei Fehler
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Validiere Username ------ */
    if (!validate_username(username)) {
        write(client_socket, "0\n", 2);
        return;
    }
    
    /* ------ SCHRITT 3: Suche alle Nachrichten ------ */
    // Array für Subjects (max 100 Nachrichten)
    char subjects[100][MAX_SUBJECT + 1];
    int count = 0;  // Anzahl gefundener Nachrichten
    int num = 1;    // Aktuelle Nachrichtennummer
    
    // Probiere Nummern 1 bis 100 durch
    while (num <= 100 && count < 100) {
        // Erstelle Dateipfad
        char msg_file[512];
        snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
                 mail_spool_dir, username, num);
        
        // Versuche Datei zu öffnen
        FILE *fp = fopen(msg_file, "r");
        
        if (fp != NULL) {
            // Datei existiert → lese Subject
            char line[BUFFER_SIZE];
            
            // Überspringe erste beiden Zeilen (Sender, Receiver)
            fgets(line, sizeof(line), fp);  // Sender
            fgets(line, sizeof(line), fp);  // Receiver
            
            // Lese Subject-Zeile
            if (fgets(line, sizeof(line), fp) != NULL) {
                // Prüfe ob Zeile mit "Subject: " beginnt
                if (strncmp(line, "Subject: ", 9) == 0) {
                    // Kopiere Subject (ohne "Subject: " Präfix)
                    strncpy(subjects[count], line + 9, MAX_SUBJECT);
                    subjects[count][strcspn(subjects[count], "\n")] = 0;
                    count++;  // Zähle gefundene Nachricht
                }
            }
            
            fclose(fp);
        }
        
        num++;  // Nächste Nummer probieren
    }
    
    /* ------ SCHRITT 4: Sende Anzahl ------ */
    char response[32];
    snprintf(response, sizeof(response), "%d\n", count);
    write(client_socket, response, strlen(response));
    
    /* ------ SCHRITT 5: Sende alle Subjects ------ */
    for (int i = 0; i < count; i++) {
        write(client_socket, subjects[i], strlen(subjects[i]));
        write(client_socket, "\n", 1);
    }
}

/**
 * FUNKTION: handle_read
 * ---------------------
 * Verarbeitet den READ-Befehl vom Client.
 * 
 * PROTOKOLL:
 * 1. "READ\n" (bereits empfangen)
 * 2. "<username>\n"
 * 3. "<nachrichtennummer>\n"
 * → Server antwortet:
 * 4. "OK\n" oder "ERR\n"
 * 5. Bei OK: kompletter Nachrichteninhalt
 * 
 * PARAMETER:
 *   client_socket - Socket des Clients
 */
void handle_read(int client_socket) {
    char username[MAX_USERNAME + 2];
    char msg_num_str[32];
    
    /* ------ SCHRITT 1: Empfange Username ------ */
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Empfange Nachrichtennummer ------ */
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    /* ------ SCHRITT 3: Validiere Username ------ */
    if (!validate_username(username)) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 4: Konvertiere Nummer zu Integer ------ */
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 5: Öffne Nachrichtendatei ------ */
    char msg_file[512];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
             mail_spool_dir, username, msg_num);
    
    FILE *fp = fopen(msg_file, "r");
    if (fp == NULL) {
        // Datei existiert nicht
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 6: Sende Erfolg ------ */
    write(client_socket, "OK\n", 3);
    
    /* ------ SCHRITT 7: Sende gesamten Dateiinhalt ------ */
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        write(client_socket, line, strlen(line));
    }
    
    fclose(fp);
}

/**
 * FUNKTION: handle_del
 * --------------------
 * Verarbeitet den DEL-Befehl vom Client.
 * 
 * PROTOKOLL:
 * 1. "DEL\n" (bereits empfangen)
 * 2. "<username>\n"
 * 3. "<nachrichtennummer>\n"
 * → Server antwortet:
 * 4. "OK\n" oder "ERR\n"
 * 
 * PARAMETER:
 *   client_socket - Socket des Clients
 */
void handle_del(int client_socket) {
    char username[MAX_USERNAME + 2];
    char msg_num_str[32];
    
    /* ------ SCHRITT 1: Empfange Username ------ */
    if (readline(client_socket, username, sizeof(username)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Empfange Nachrichtennummer ------ */
    if (readline(client_socket, msg_num_str, sizeof(msg_num_str)) <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    /* ------ SCHRITT 3: Validiere Username ------ */
    if (!validate_username(username)) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 4: Konvertiere Nummer ------ */
    int msg_num = atoi(msg_num_str);
    if (msg_num <= 0) {
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 5: Lösche Datei ------ */
    char msg_file[512];
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
             mail_spool_dir, username, msg_num);
    
    // unlink() löscht eine Datei
    if (unlink(msg_file) == -1) {
        // Löschen fehlgeschlagen (Datei existiert nicht oder Fehler)
        write(client_socket, "ERR\n", 4);
        return;
    }
    
    /* ------ SCHRITT 6: Erfolg ------ */
    write(client_socket, "OK\n", 3);
}

/**
 * FUNKTION: handle_client
 * -----------------------
 * Hauptfunktion für die Client-Behandlung.
 * Empfängt Befehle in einer Schleife und ruft entsprechende Handler auf.
 * 
 * ABLAUF:
 * 1. Warte auf Befehl vom Client
 * 2. Identifiziere Befehl (SEND, LIST, READ, DEL, QUIT)
 * 3. Rufe entsprechenden Handler auf
 * 4. Wiederhole bis QUIT oder Verbindungsabbruch
 * 
 * PARAMETER:
 *   client_socket - Socket des Clients
 */
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    
    // Befehls-Empfangs-Schleife
    while (1) {
        /* ------ Empfange Befehl ------ */
        int len = readline(client_socket, buffer, sizeof(buffer));
        
        if (len <= 0) {
            // Connection geschlossen oder Fehler
            break;
        }
        
        // Entferne Newline für Vergleich
        buffer[strcspn(buffer, "\n")] = 0;
        
        printf("Received command: %s\n", buffer);
        
        /* ------ Verarbeite Befehl ------ */
        // strcmp() vergleicht zwei Strings
        // Rückgabe 0 = Strings sind gleich
        
        if (strcmp(buffer, "SEND") == 0) {
            handle_send(client_socket);
        } 
        else if (strcmp(buffer, "LIST") == 0) {
            handle_list(client_socket);
        } 
        else if (strcmp(buffer, "READ") == 0) {
            handle_read(client_socket);
        } 
        else if (strcmp(buffer, "DEL") == 0) {
            handle_del(client_socket);
        } 
        else if (strcmp(buffer, "QUIT") == 0) {
            printf("Client disconnected\n");
            break;  // Beende Schleife
        } 
        else {
            // Unbekannter Befehl
            write(client_socket, "ERR\n", 4);
        }
    }
}

/* ============================================================================
 * HAUPTPROGRAMM
 * ============================================================================ */

/**
 * FUNKTION: main
 * --------------
 * Einstiegspunkt des Server-Programms.
 * 
 * ABLAUF:
 * 1. Argumente prüfen und auslesen
 * 2. Mail-Spool-Verzeichnis erstellen
 * 3. Server-Socket erstellen und binden
 * 4. Auf Verbindungen warten (listen)
 * 5. Clients nacheinander behandeln (iterativer Server)
 * 
 * HINWEIS:
 * Dies ist ein ITERATIVER Server - er behandelt Clients nacheinander,
 * nicht parallel. Für viele Clients sollte ein Fork oder Thread-basierter
 * Ansatz verwendet werden.
 */
int main(int argc, char *argv[]) {
    
    /* ------ SCHRITT 1: Argumente prüfen ------ */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <mail-spool-directory>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 2: Parameter auslesen ------ */
    int port = atoi(argv[1]);
    
    // Kopiere Mail-Spool-Pfad in globale Variable
    strncpy(mail_spool_dir, argv[2], sizeof(mail_spool_dir) - 1);
    mail_spool_dir[sizeof(mail_spool_dir) - 1] = '\0';  // Null-Terminierung
    
    /* ------ SCHRITT 3: Mail-Spool-Verzeichnis erstellen ------ */
    struct stat st = {0};
    if (stat(mail_spool_dir, &st) == -1) {
        // Verzeichnis existiert nicht → erstellen
        if (mkdir(mail_spool_dir, 0700) == -1) { // 0700 = rwx für Owner
            perror("Failed to create mail spool directory");
            return EXIT_FAILURE;
        }
    }
    
    /* ------ SCHRITT 4: Server-Socket erstellen ------ */
    // AF_INET = IPv4, SOCK_STREAM = TCP
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 5: Socket-Option setzen ------ */
    // SO_REUSEADDR erlaubt schnelles Neustarten des Servers
    // Ohne diese Option müsste man nach Beenden mehrere Minuten warten
    int opt = 1;

    int sockopt_result = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (sockopt_result == -1) {
        perror("setsockopt");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 6: Server-Adresse konfigurieren ------ */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Alle Netzwerk-Interfaces
    server_addr.sin_port = htons(port);       // Port (Host TO Network Short)
    
    /* ------ SCHRITT 7: Socket an Adresse binden ------ */
    // bind() verknüpft Socket mit Port
    if (bind(server_socket, (struct sockaddr *)&server_addr, 
             sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 8: Auf Verbindungen lauschen ------ */
    // listen() markiert Socket als passiv (wartet auf Verbindungen)
    // 5 = Backlog (max. Anzahl wartender Verbindungen)
    if (listen(server_socket, 5) == -1) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    printf("TW-Mailer Server listening on port %d\n", port);
    printf("Mail spool directory: %s\n", mail_spool_dir);
    
    /* ------ SCHRITT 9: Client-Accept-Schleife ------ */
    // Endlosschleife: Server läuft kontinuierlich
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        /* ------ Warte auf Client-Verbindung ------ */
        // accept() blockiert bis Client sich verbindet
        int client_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &client_addr_len);
        
        if (client_socket == -1) {
            perror("accept");
            continue;  // Fehler ignorieren, nächsten Client abwarten
        }
        
        // Zeige Client-Informationen
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr),  // IP zu String
               ntohs(client_addr.sin_port));     // Port (Network TO Host Short)
        
        /* ------ Behandle Client-Anfragen ------ */
        handle_client(client_socket);
        
        /* ------ Schließe Client-Verbindung ------ */
        close(client_socket);
        printf("Client connection closed\n");
    }
    
    /* ------ Aufräumen ------ */
    // Wird nie erreicht (Endlosschleife), aber guter Stil
    close(server_socket);
    return EXIT_SUCCESS;
}
