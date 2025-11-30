// Usage: ./twmailer-client <ip> <port>
// Pro Version: LOGIN required, automatic username for SEND/LIST/READ/DEL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80
#define MAX_MESSAGE_SIZE 8192
#define MAX_LDAP_USERNAME 256
#define MAX_PASSWORD 256

// Globale Variable für Session-Status
int is_logged_in = 0;

// Liest eine Zeile vom Socket (bis \n)
// return: Anzahl gelesener Zeichen, -1 bei Fehler, 0 wenn Connection geschlossen
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
                break; // Newline gefunden
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
    
    buffer[i] = '\0'; 
    return i;
}

// Return: 0 if success, -1 if input zu lange, -2 if error
int read_input(char *buffer, int size) {
    // Sicher mit fgets lesen
    if (fgets(buffer, size, stdin) == NULL) {
        return -2;
    }
    // Prüfe auf Newline und entferne es
    char *p = strchr(buffer, '\n');
    if (p) {
        *p = '\0';
        return 0;
    }
    // Prüfe ob Input in Buffer passt
    if (strlen(buffer) < (size_t)(size - 1)) {
        return 0;
    }
    // Wenn Input zu lange: Rest der Zeile einlesen
    int c = getchar();
    if (c == '\n' || c == EOF) {
        return 0;
    }
    while (c != '\n' && c != EOF) {
        c = getchar();
    }
    return -1;
}

// Liest Passwort ohne Echo (sicher)
int read_password(char *buffer, int size) {
    struct termios old_term, new_term;
    
    // Speichere alte Terminal-Einstellungen
    if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
        // Fallback: normales Lesen wenn Terminal nicht verfügbar
        return read_input(buffer, size);
    }
    
    // Deaktiviere Echo
    new_term = old_term;
    new_term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    
    // Lese Passwort
    int result = read_input(buffer, size);
    
    // Stelle alte Einstellungen wieder her
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    
    printf("\n"); // Neue Zeile nach Passwort
    
    return result;
}

// Sendet LOGIN-Command an Server
void login(int socket_fd) {
    char username[MAX_LDAP_USERNAME + 1];
    char password[MAX_PASSWORD + 1];
    char response[16];
    
    printf("\n=== LOGIN ===\n");
    
    if (is_logged_in) {
        printf("Already logged in!\n");
        return;
    }
    
    // Lese LDAP Username
    printf("LDAP Username: ");
    int res = read_input(username, sizeof(username));
    if (res == -1) {
        printf("Error: Username too long\n");
        return;
    } else if (res == -2) {
        printf("Error reading input\n");
        return;
    }
    
    if (strlen(username) == 0) {
        printf("Error: Username cannot be empty\n");
        return;
    }
    
    // Lese Passwort (ohne Echo)
    printf("Password: ");
    res = read_password(password, sizeof(password));
    if (res == -1) {
        printf("Error: Password too long\n");
        return;
    } else if (res == -2) {
        printf("Error reading input\n");
        return;
    }
    
    // Sende LOGIN Command
    write(socket_fd, "LOGIN\n", 6);
    
    // Sende Username
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
    // Sende Password
    write(socket_fd, password, strlen(password));
    write(socket_fd, "\n", 1);
    
    // Lese Response
    if (readline(socket_fd, response, sizeof(response)) > 0) {
        response[strcspn(response, "\n")] = 0;
        if (strcmp(response, "OK") == 0) {
            printf("Login successful!\n");
            is_logged_in = 1;
        } else {
            printf("Login failed! Check your credentials.\n");
            printf("(After 3 failed attempts, your IP will be blocked for 1 minute)\n");
        }
    } else {
        printf("Error reading response from server\n");
    }
}

// Sendet SEND-Command an Server (Pro Version: kein Sender nötig, kommt aus Session)
void send_message(int socket_fd) {
    char receiver[MAX_USERNAME + 1];
    char subject[MAX_SUBJECT + 1];
    char buffer[BUFFER_SIZE];
    char message[MAX_MESSAGE_SIZE];

    printf("\n=== SEND MESSAGE ===\n");
    
    if (!is_logged_in) {
        printf("Error: You must login first!\n");
        return;
    }

    int res;
    
    // Lese Receiver
    while (1) {
        printf("Receiver (max %d chars, a-z, 0-9): ", MAX_USERNAME);
        res = read_input(receiver, sizeof(receiver));
        if (res == -1) {
            printf("Error: Username too long (max %d chars)\n", MAX_USERNAME);
            return;
        } else if (res == -2) {
            printf("Error reading input\n");
            return;
        }
        
        if (strlen(receiver) > 0) {
            break;
        }
        printf("Error: Receiver cannot be empty\n");
    }
    
    // Lese Subject
    while (1) {
        printf("Subject (max %d chars): ", MAX_SUBJECT);
        res = read_input(subject, sizeof(subject));
        if (res == -1) {
            printf("Error: Subject too long (max %d chars)\n", MAX_SUBJECT);
            return;
        } else if (res == -2) {
            printf("Error reading input\n");
            return;
        }
        
        if (strlen(subject) > 0) {
            break;
        }
        printf("Error: Subject cannot be empty\n");
    }
    
    // Lese Message
    while (1) {
        printf("Message (end with a line containing only a dot '.'):\n");
        message[0] = '\0';
        int message_len = 0;
        
        while (1) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            
            if (strcmp(buffer, ".\n") == 0) {
                break;
            }
            
            int line_len = strlen(buffer);
            if (message_len + line_len < MAX_MESSAGE_SIZE - 1) {
                strcat(message, buffer);
                message_len += line_len;
            } else {
                printf("Warning: Message buffer full, truncating...\n");
            }
        }
        
        if (strlen(message) > 0) {
            break;
        }
        printf("Error: Message cannot be empty\n");
    }
    
    // Sende SEND-Command
    write(socket_fd, "SEND\n", 5);
    
    // Sende Receiver (Sender kommt aus Session am Server)
    write(socket_fd, receiver, strlen(receiver));
    write(socket_fd, "\n", 1);
    
    // Sende Subject
    write(socket_fd, subject, strlen(subject));
    write(socket_fd, "\n", 1);
    
    // Sende Message
    write(socket_fd, message, strlen(message));
    write(socket_fd, ".\n", 2);
    
    // Lese Response vom Server
    char response[16];
    if (readline(socket_fd, response, sizeof(response)) > 0) {
        response[strcspn(response, "\n")] = 0;
        if (strcmp(response, "OK") == 0) {
            printf("Message sent successfully!\n");
        } else {
            printf("Error sending message\n");
        }
    } else {
        printf("Error reading response from server\n");
    }
}

// Sendet LIST-Command an Server (Pro Version: Username aus Session)
void list_messages(int socket_fd) {
    char buffer[BUFFER_SIZE];
    
    printf("\n=== LIST MESSAGES ===\n");
    
    if (!is_logged_in) {
        printf("Error: You must login first!\n");
        return;
    }
    
    // Sende LIST-Command (Username kommt aus Session am Server)
    write(socket_fd, "LIST\n", 5);
    
    // Lese Anzahl der Messages
    if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
        printf("Error reading response from server\n");
        return;
    }
    
    int count = atoi(buffer);
    printf("\nNumber of messages: %d\n", count);
    
    // Lese und zeige alle Subjects
    if (count > 0) {
        printf("\nSubjects:\n");
        for (int i = 0; i < count; i++) {
            if (readline(socket_fd, buffer, sizeof(buffer)) > 0) {
                buffer[strcspn(buffer, "\n")] = 0;
                printf("%d. %s\n", i + 1, buffer);
            }
        }
    }
}

// Sendet READ-Command an Server (Pro Version: Username aus Session)
void read_message(int socket_fd) {
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== READ MESSAGE ===\n");
    
    if (!is_logged_in) {
        printf("Error: You must login first!\n");
        return;
    }
    
    // Lese Message-Nummer
    printf("Message number: ");
    if (read_input(msg_num_str, sizeof(msg_num_str)) != 0) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Sende READ-Command (Username kommt aus Session am Server)
    write(socket_fd, "READ\n", 5);
    
    // Sende Message-Nummer
    write(socket_fd, msg_num_str, strlen(msg_num_str));
    write(socket_fd, "\n", 1);
    
    // Lese Response
    if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
        printf("Error reading response from server\n");
        return;
    }
    
    buffer[strcspn(buffer, "\n")] = 0;
    if (strcmp(buffer, "OK") == 0) {
        // OK erhalten, lese Message-Inhalt
        printf("\n--- Message Content ---\n");
        
        // Lese Nachricht Zeile für Zeile
        // Format: Sender: ...\nReceiver: ...\nSubject: ...\nMessage:\n<text>
        int lines_read = 0;
        printf("(Press any key to stop reading)\n");
        while (lines_read < 1000) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(socket_fd, &readfds);
            FD_SET(STDIN_FILENO, &readfds);

            if (select(socket_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                break;
            }

            if (FD_ISSET(socket_fd, &readfds)) {
                if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
                    break;
                }
                printf("%s", buffer);
                lines_read++;
            }
        }
        
        printf("--- End of Message ---\n");
    } else {
        printf("Error: Message not found or invalid request\n");
    }
}

// Sendet DEL-Command an Server (Pro Version: Username aus Session)
void delete_message(int socket_fd) {
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== DELETE MESSAGE ===\n");
    
    if (!is_logged_in) {
        printf("Error: You must login first!\n");
        return;
    }
    
    // Lese Message-Nummer
    printf("Message number: ");
    if (read_input(msg_num_str, sizeof(msg_num_str)) != 0) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Sende DEL-Command (Username kommt aus Session am Server)
    write(socket_fd, "DEL\n", 4);
    
    // Sende Message-Nummer
    write(socket_fd, msg_num_str, strlen(msg_num_str));
    write(socket_fd, "\n", 1);
    
    // Lese Response
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

// Zeigt Menu an
void display_menu() {
    printf("\n===== TW-Mailer Pro Client =====\n");
    if (is_logged_in) {
        printf("Status: LOGGED IN\n");
    } else {
        printf("Status: NOT LOGGED IN\n");
    }
    printf("--------------------------------\n");
    printf("1. LOGIN - Authenticate with LDAP\n");
    printf("2. SEND  - Send a message\n");
    printf("3. LIST  - List your messages\n");
    printf("4. READ  - Read a specific message\n");
    printf("5. DEL   - Delete a message\n");
    printf("6. QUIT  - Exit the client\n");
    printf("================================\n");
    printf("Choose an option: ");
}

int main(int argc, char *argv[]) {
    // Prüfe Argumente
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Lese Server-IP und Port
    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // localhost erlauben
    if (strcmp(server_ip, "localhost") == 0) {
        server_ip = "127.0.0.1";
    }
    
    // Erstelle Socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    
    // Verbinde mit Server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(server_port); // Portnummer konvertieren zu Netzwerk-Byte-Reihenfolge
    
    // Konvertiere IP-Adresse
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    
    // Verbindung herstellen
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    
    printf("Connected to TW-Mailer Pro server at %s:%d\n", server_ip, server_port);
    printf("Please login first to use the mail functions.\n");
    
    char choice[16];
    
    // Haupt-Loop: Zeige Menu und verarbeite User-Input
    while (1) {
        display_menu();
        
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            break;
        }
        
        int option = atoi(choice);
        
        // Verarbeite gewählte Option
        switch (option) {
            case 1:
                login(socket_fd);
                break;
            case 2:
                send_message(socket_fd);
                break;
            case 3:
                list_messages(socket_fd);
                break;
            case 4:
                read_message(socket_fd);
                break;
            case 5:
                delete_message(socket_fd);
                break;
            case 6:
                // QUIT: Sende Command und beende
                printf("Sending QUIT command...\n");
                write(socket_fd, "QUIT\n", 5);
                printf("Goodbye!\n");
                close(socket_fd);
                return EXIT_SUCCESS;
            default:
                printf("Invalid option. Please try again.\n");
        }
    }
    
    // Socket schließen
    close(socket_fd);
    return EXIT_SUCCESS;
}
