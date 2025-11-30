/*
 * ============================================================================
 * TW-MAILER CLIENT - AUSFÜHRLICH KOMMENTIERTE VERSION
 * ============================================================================
 * 
 * ZWECK:
 * Diese Datei ist eine kommentierte Version des TW-Mailer Clients.
 * Sie dient dem besseren Verständnis der Funktionsweise.
 * 
 * VERWENDUNG:
 * ./twmailer-client <server-ip> <port>
 * 
 * BEISPIEL:
 * ./twmailer-client 127.0.0.1 6543
 * 
 * FUNKTIONSWEISE:
 * Der Client stellt eine TCP-Verbindung zum Server her und bietet dem
 * Benutzer ein Menü zur Auswahl verschiedener Befehle (SEND, LIST, READ, DEL).
 * Jeder Befehl folgt einem spezifischen Protokoll bei der Kommunikation
 * mit dem Server.
 * ============================================================================
 */

#include <stdio.h>      // Standard Input/Output (printf, fgets, etc.)
#include <stdlib.h>     // Standard Library (exit, atoi, etc.)
#include <string.h>     // String-Funktionen (strlen, strcmp, strcspn, etc.)
#include <unistd.h>     // UNIX Standard (read, write, close)
#include <sys/socket.h> // Socket-Programmierung
#include <netinet/in.h> // Internet-Adressen (sockaddr_in)
#include <arpa/inet.h>  // IP-Adressen-Konvertierung (inet_pton)
#include <sys/select.h> // select() für Non-Blocking I/O
#include <sys/time.h>   // timeval Struktur für Timeouts

/* ============================================================================
 * KONSTANTEN
 * ============================================================================ */

// Maximale Größe für Lese-/Schreib-Buffer
#define BUFFER_SIZE 1024

// Maximale Länge für Benutzernamen (gemäß Protokoll)
#define MAX_USERNAME 8

// Maximale Länge für Betreff-Zeile (gemäß Protokoll)
#define MAX_SUBJECT 80

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

/**
 * FUNKTION: readline
 * -------------------
 * Liest eine Zeile vom Socket bis zum Newline-Zeichen (\n).
 * 
 * WARUM DIESE FUNKTION?
 * Die Standard-read()-Funktion liest eine bestimmte Anzahl von Bytes,
 * aber wir brauchen Zeilen (bis \n). Diese Funktion liest Zeichen für
 * Zeichen, bis ein Newline gefunden wird.
 * 
 * PARAMETER:
 *   fd       - File Descriptor des Sockets
 *   buffer   - Zielpuffer für die gelesene Zeile
 *   max_len  - Maximale Anzahl zu lesender Zeichen
 * 
 * RÜCKGABE:
 *   > 0      - Anzahl der gelesenen Zeichen (Erfolg)
 *   0        - Connection wurde vom Server geschlossen
 *   -1       - Fehler beim Lesen
 * 
 * WICHTIG:
 * Die Funktion terminiert den String immer mit '\0', sodass er als
 * C-String verwendet werden kann.
 */
int readline(int fd, char *buffer, int max_len) {
    int i = 0;           // Aktueller Index im Buffer
    char c;              // Aktuell gelesenes Zeichen
    int n;               // Anzahl gelesener Bytes von read()
    
    // Schleife: Lese Zeichen für Zeichen, bis Newline oder max_len
    while (i < max_len - 1) {  // -1 wegen '\0' am Ende
        
        // Lese genau 1 Byte vom Socket
        n = read(fd, &c, 1);
        
        if (n == 1) {
            // Erfolgreich 1 Zeichen gelesen
            buffer[i++] = c;  // Speichere Zeichen im Buffer
            
            // Prüfe ob Newline erreicht
            if (c == '\n') {
                break;  // Zeile vollständig, beende Schleife
            }
        } 
        else if (n == 0) {
            // read() gab 0 zurück → Connection wurde geschlossen
            if (i == 0) {
                return 0;  // Keine Daten gelesen
            }
            break;  // Beende Schleife mit den bisher gelesenen Daten
        } 
        else {
            // n < 0 → Fehler beim Lesen
            return -1;
        }
    }
    
    // Null-Terminierung: Macht buffer zu einem gültigen C-String
    buffer[i] = '\0';
    
    // Gib Anzahl der gelesenen Zeichen zurück
    return i;
}

/* ============================================================================
 * PROTOKOLL-FUNKTIONEN
 * ============================================================================ */

/**
 * FUNKTION: send_message
 * ----------------------
 * Implementiert den SEND-Befehl des TW-Mailer Protokolls.
 * 
 * PROTOKOLL-ABLAUF:
 * 1. Client sendet: "SEND\n"
 * 2. Client sendet: "<sender>\n"
 * 3. Client sendet: "<empfänger>\n"
 * 4. Client sendet: "<betreff>\n"
 * 5. Client sendet: "<nachricht-zeile-1>\n"
 *    Client sendet: "<nachricht-zeile-2>\n"
 *    ...
 *    Client sendet: ".\n"  (Punkt markiert Ende)
 * 6. Server antwortet: "OK\n" oder "ERR\n"
 * 
 * PARAMETER:
 *   socket_fd - File Descriptor der Socket-Verbindung zum Server
 * 
 * BESONDERHEITEN:
 * - Der Benutzer wird nach allen erforderlichen Daten gefragt
 * - Die Nachricht endet mit einer Zeile, die nur einen Punkt enthält
 * - Validierung findet hauptsächlich auf dem Server statt
 */
void send_message(int socket_fd) {
    // Lokale Buffer für Benutzereingaben
    char sender[MAX_USERNAME + 1];      // +1 für '\0'
    char receiver[MAX_USERNAME + 1];
    char subject[MAX_SUBJECT + 1];
    char line[BUFFER_SIZE];
    
    printf("\n=== SEND MESSAGE ===\n");
    
    /* ------ SCHRITT 1: Sender eingeben ------ */
    printf("Sender (max %d chars, a-z, 0-9): ", MAX_USERNAME);
    
    // fgets() liest eine Zeile von stdin (Benutzereingabe)
    if (fgets(sender, sizeof(sender), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    
    // strcspn() findet Position des Newline, ersetze es mit '\0'
    // WARUM? fgets() speichert auch das \n, was wir nicht wollen
    sender[strcspn(sender, "\n")] = 0;
    
    /* ------ SCHRITT 2: Empfänger eingeben ------ */
    printf("Receiver (max %d chars, a-z, 0-9): ", MAX_USERNAME);
    if (fgets(receiver, sizeof(receiver), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    receiver[strcspn(receiver, "\n")] = 0;
    
    /* ------ SCHRITT 3: Betreff eingeben ------ */
    printf("Subject (max %d chars): ", MAX_SUBJECT);
    if (fgets(subject, sizeof(subject), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    subject[strcspn(subject, "\n")] = 0;
    
    printf("Message (end with a line containing only a dot '.'):\n");
    
    /* ------ SCHRITT 4: Sende SEND-Befehl ------ */
    // write() sendet Daten über den Socket
    // Syntax: write(file_descriptor, daten, anzahl_bytes)
    write(socket_fd, "SEND\n", 5);  // 5 = Länge von "SEND\n"
    
    /* ------ SCHRITT 5: Sende Sender ------ */
    write(socket_fd, sender, strlen(sender));
    write(socket_fd, "\n", 1);  // Newline als Trennzeichen
    
    /* ------ SCHRITT 6: Sende Empfänger ------ */
    write(socket_fd, receiver, strlen(receiver));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 7: Sende Betreff ------ */
    write(socket_fd, subject, strlen(subject));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 8: Sende Nachrichtentext ------ */
    // Endlosschleife, die auf den Punkt "." wartet
    while (1) {
        // Lese nächste Zeile von Benutzereingabe
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;  // EOF oder Fehler
        }
        
        // Sende Zeile an Server
        write(socket_fd, line, strlen(line));
        
        // Prüfe ob Ende-Marker erreicht (".\n")
        if (strcmp(line, ".\n") == 0) {
            break;  // Nachricht vollständig, beende Schleife
        }
    }
    
    /* ------ SCHRITT 9: Empfange Antwort vom Server ------ */
    char response[16];
    if (readline(socket_fd, response, sizeof(response)) > 0) {
        // Entferne Newline für Vergleich
        response[strcspn(response, "\n")] = 0;
        
        // Prüfe ob Server "OK" gesendet hat
        if (strcmp(response, "OK") == 0) {
            printf("Message sent successfully!\n");
        } else {
            printf("Error sending message\n");
        }
    } else {
        printf("Error reading response from server\n");
    }
}

/**
 * FUNKTION: list_messages
 * -----------------------
 * Implementiert den LIST-Befehl des TW-Mailer Protokolls.
 * 
 * PROTOKOLL-ABLAUF:
 * 1. Client sendet: "LIST\n"
 * 2. Client sendet: "<username>\n"
 * 3. Server antwortet: "<anzahl>\n"
 * 4. Server sendet: "<betreff-1>\n"
 *    Server sendet: "<betreff-2>\n"
 *    ... (anzahl Zeilen)
 * 
 * ZWECK:
 * Zeigt alle Betreffs der Nachrichten für einen Benutzer an.
 * Der Benutzer sieht so eine Übersicht seiner Postfach-Inhalte.
 * 
 * PARAMETER:
 *   socket_fd - File Descriptor der Socket-Verbindung
 */
void list_messages(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== LIST MESSAGES ===\n");
    
    /* ------ SCHRITT 1: Username eingeben ------ */
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Sende LIST-Befehl ------ */
    write(socket_fd, "LIST\n", 5);
    
    /* ------ SCHRITT 3: Sende Username ------ */
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 4: Empfange Anzahl der Nachrichten ------ */
    if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
        printf("Error reading response from server\n");
        return;
    }
    
    // atoi() konvertiert String zu Integer
    int count = atoi(buffer);
    printf("\nNumber of messages: %d\n", count);
    
    /* ------ SCHRITT 5: Empfange und zeige alle Betreffs ------ */
    if (count > 0) {
        printf("\nSubjects:\n");
        
        // Schleife durch alle Nachrichten
        for (int i = 0; i < count; i++) {
            if (readline(socket_fd, buffer, sizeof(buffer)) > 0) {
                // Entferne Newline für saubere Ausgabe
                buffer[strcspn(buffer, "\n")] = 0;
                
                // Zeige nummerierte Liste (i+1, weil bei 1 beginnen)
                printf("%d. %s\n", i + 1, buffer);
            }
        }
    }
}

/**
 * FUNKTION: read_message
 * ----------------------
 * Implementiert den READ-Befehl des TW-Mailer Protokolls.
 * 
 * PROTOKOLL-ABLAUF:
 * 1. Client sendet: "READ\n"
 * 2. Client sendet: "<username>\n"
 * 3. Client sendet: "<nachrichtennummer>\n"
 * 4. Server antwortet: "OK\n" oder "ERR\n"
 * 5. Bei OK: Server sendet komplette Nachricht bis Connection-Ende
 * 
 * ZWECK:
 * Zeigt den vollständigen Inhalt einer bestimmten Nachricht an.
 * 
 * PARAMETER:
 *   socket_fd - File Descriptor der Socket-Verbindung
 */
void read_message(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char msg_num_str[16];  // String für Nachrichtennummer
    char buffer[BUFFER_SIZE];
    
    printf("\n=== READ MESSAGE ===\n");
    
    /* ------ SCHRITT 1: Username eingeben ------ */
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Nachrichtennummer eingeben ------ */
    printf("Message number: ");
    if (fgets(msg_num_str, sizeof(msg_num_str), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    /* ------ SCHRITT 3: Sende READ-Befehl ------ */
    write(socket_fd, "READ\n", 5);
    
    /* ------ SCHRITT 4: Sende Username ------ */
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 5: Sende Nachrichtennummer ------ */
    write(socket_fd, msg_num_str, strlen(msg_num_str));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 6: Empfange Antwort ------ */
    if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
        printf("Error reading response from server\n");
        return;
    }
    
    // Entferne Newline und prüfe Antwort
    buffer[strcspn(buffer, "\n")] = 0;
    
    if (strcmp(buffer, "OK") == 0) {
        // Server sendet Nachricht → Empfange und zeige alles
        printf("\n--- Message Content ---\n");
        

        int lines_read = 0;
        while (readline(socket_fd, buffer, sizeof(buffer)) > 0 && lines_read < 1000) {
            printf("%s", buffer);  // Zeige Zeile (enthält bereits \n)
            lines_read++;
            
            if (lines_read >= 4 && strlen(buffer) <= 2) {
                // Kurze Wartezeit für weitere Daten
                // fd_set = File Descriptor Set (Menge von File Descriptors)
                fd_set readfds;
                struct timeval tv; 
                
                FD_ZERO(&readfds);           // Leere die Menge
                FD_SET(socket_fd, &readfds); // Füge unseren Socket hinzu
                
                tv.tv_sec = 0;         // 0 Sekunden
                tv.tv_usec = 10000;   // 10ms
                
                if (select(socket_fd + 1, &readfds, NULL, NULL, &tv) <= 0) {
                    // Keine weiteren Daten innerhalb 100ms
                    break;
                }
            }
        }
        
        printf("--- End of Message ---\n");
    } else {
        // Server hat "ERR" gesendet
        printf("Error: Message not found or invalid request\n");
    }
}

/**
 * FUNKTION: delete_message
 * ------------------------
 * Implementiert den DEL-Befehl des TW-Mailer Protokolls.
 * 
 * PROTOKOLL-ABLAUF:
 * 1. Client sendet: "DEL\n"
 * 2. Client sendet: "<username>\n"
 * 3. Client sendet: "<nachrichtennummer>\n"
 * 4. Server antwortet: "OK\n" oder "ERR\n"
 * 
 * ZWECK:
 * Löscht eine bestimmte Nachricht aus dem Postfach.
 * 
 * PARAMETER:
 *   socket_fd - File Descriptor der Socket-Verbindung
 */
void delete_message(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== DELETE MESSAGE ===\n");
    
    /* ------ SCHRITT 1: Username eingeben ------ */
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    /* ------ SCHRITT 2: Nachrichtennummer eingeben ------ */
    printf("Message number: ");
    if (fgets(msg_num_str, sizeof(msg_num_str), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    /* ------ SCHRITT 3: Sende DEL-Befehl ------ */
    write(socket_fd, "DEL\n", 4);  // Nur 4 Bytes: "DEL\n"
    
    /* ------ SCHRITT 4: Sende Username ------ */
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 5: Sende Nachrichtennummer ------ */
    write(socket_fd, msg_num_str, strlen(msg_num_str));
    write(socket_fd, "\n", 1);
    
    /* ------ SCHRITT 6: Empfange Antwort ------ */
    if (readline(socket_fd, buffer, sizeof(buffer)) > 0) {
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strcmp(buffer, "OK") == 0) {
            printf("Message deleted successfully!\n");
        } else {
            printf("Error deleting message\n");
        }
    } else {
        printf("Error reading response from server\n");
    }
}

/**
 * FUNKTION: display_menu
 * ----------------------
 * Zeigt das Hauptmenü für den Benutzer an.
 * 
 * ZWECK:
 * Gibt dem Benutzer eine Übersicht über alle verfügbaren Befehle.
 */
void display_menu() {
    printf("\n===== TW-Mailer Client Menu =====\n");
    printf("1. SEND - Send a message\n");
    printf("2. LIST - List messages for a user\n");
    printf("3. READ - Read a specific message\n");
    printf("4. DEL  - Delete a message\n");
    printf("5. QUIT - Exit the client\n");
    printf("================================\n");
    printf("Choose an option: ");
}

/* ============================================================================
 * HAUPTPROGRAMM
 * ============================================================================ */

/**
 * FUNKTION: main
 * --------------
 * Einstiegspunkt des Programms.
 * 
 * ABLAUF:
 * 1. Kommandozeilen-Argumente prüfen
 * 2. Socket erstellen
 * 3. Verbindung zum Server herstellen
 * 4. Menü-Schleife: Benutzer kann Befehle ausführen
 * 5. Verbindung schließen und beenden
 * 
 * ARGUMENTE:
 *   argc - Anzahl der Kommandozeilen-Argumente
 *   argv - Array mit den Argumenten
 *          argv[0] = Programmname
 *          argv[1] = Server-IP
 *          argv[2] = Server-Port
 */
int main(int argc, char *argv[]) {
    
    /* ------ SCHRITT 1: Argumente validieren ------ */
    // Erwarte genau 3 Argumente: ./programm <ip> <port>
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;  // Beende mit Fehlercode
    }
    
    /* ------ SCHRITT 2: Parameter auslesen ------ */
    char *server_ip = argv[1];        // IP-Adresse als String
    int server_port = atoi(argv[2]);  // Port als Integer
    
    /* ------ SCHRITT 3: Socket erstellen ------ */
    // socket() erstellt einen neuen Socket
    // AF_INET      = IPv4 Internet-Protokoll
    // SOCK_STREAM  = TCP (zuverlässige, verbindungsorientierte Übertragung)
    // 0            = Protokoll automatisch wählen (TCP für SOCK_STREAM)
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");  // perror() gibt Fehlermeldung aus
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 4: Server-Adresse konfigurieren ------ */
    struct sockaddr_in server_addr;
    
    // memset() setzt alle Bytes auf 0 (wichtig für Struktur-Initialisierung)
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;           // IPv4
    server_addr.sin_port = htons(server_port);  // htons() = Host TO Network Short
                                                 // Konvertiert Byte-Reihenfolge
    
    // inet_pton() konvertiert IP-String zu binärer Form
    // pton = Presentation TO Network
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    
    /* ------ SCHRITT 5: Verbindung zum Server herstellen ------ */
    // connect() stellt TCP-Verbindung her
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    
    printf("Connected to TW-Mailer server at %s:%d\n", server_ip, server_port);
    
    /* ------ SCHRITT 6: Haupt-Menü-Schleife ------ */
    char choice[16];  // Buffer für Benutzer-Auswahl
    
    // Endlosschleife für Menü-Interaktion
    while (1) {
        // Zeige Menü an
        display_menu();
        
        // Lese Benutzer-Auswahl
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            break;  // EOF erreicht (z.B. Ctrl+D)
        }
        
        // Konvertiere Eingabe zu Integer
        int option = atoi(choice);
        
        // Verarbeite gewählte Option mit switch-Statement
        switch (option) {
            case 1:
                // SEND: Nachricht senden
                send_message(socket_fd);
                break;
                
            case 2:
                // LIST: Nachrichten auflisten
                list_messages(socket_fd);
                break;
                
            case 3:
                // READ: Nachricht lesen
                read_message(socket_fd);
                break;
                
            case 4:
                // DEL: Nachricht löschen
                delete_message(socket_fd);
                break;
                
            case 5:
                // QUIT: Programm beenden
                printf("Sending QUIT command...\n");
                write(socket_fd, "QUIT\n", 5);
                printf("Goodbye!\n");
                
                // Socket schließen (wichtig!)
                close(socket_fd);
                return EXIT_SUCCESS;  // Normales Programmende
                
            default:
                // Ungültige Eingabe
                printf("Invalid option. Please try again.\n");
        }
    }
    
    /* ------ SCHRITT 7: Aufräumen ------ */
    // Socket schließen (wird nur erreicht bei EOF in Schleife)
    close(socket_fd);
    return EXIT_SUCCESS;
}
