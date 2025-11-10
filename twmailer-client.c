// Usage: ./twmailer-client <ip> <port>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80

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

// Sendet SEND-Command an Server
void send_message(int socket_fd) {
    char sender[MAX_USERNAME + 1];
    char receiver[MAX_USERNAME + 1];
    char subject[MAX_SUBJECT + 1];
    char line[BUFFER_SIZE];

    printf("\nSEND MESSAGE\n");

    // Lese Sender
    printf("Sender (max %d chars, a-z, 0-9): ", MAX_USERNAME);
    if (fgets(sender, sizeof(sender), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    sender[strcspn(sender, "\n")] = 0; // Entferne Newline
    
    // Lese Receiver
    printf("Receiver (max %d chars, a-z, 0-9): ", MAX_USERNAME);
    if (fgets(receiver, sizeof(receiver), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    receiver[strcspn(receiver, "\n")] = 0;
    
    // Lese Subject
    printf("Subject (max %d chars): ", MAX_SUBJECT);
    if (fgets(subject, sizeof(subject), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    subject[strcspn(subject, "\n")] = 0;
    
    printf("Message (end with a line containing only a dot '.'):\n");
    
    // Sende SEND-Command
    write(socket_fd, "SEND\n", 5);
    
    // Sende Sender
    write(socket_fd, sender, strlen(sender));
    write(socket_fd, "\n", 1);
    
    // Sende Receiver
    write(socket_fd, receiver, strlen(receiver));
    write(socket_fd, "\n", 1);
    
    // Sende Subject
    write(socket_fd, subject, strlen(subject));
    write(socket_fd, "\n", 1);
    
    // Sende Message-Zeilen
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        write(socket_fd, line, strlen(line));
        
        // Prüfe auf Ende ".\n"
        if (strcmp(line, ".\n") == 0) {
            break;
        }
    }
    
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

// Sendet LIST-Command an Server
void list_messages(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== LIST MESSAGES ===\n");
    
    // Lese Username
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Sende LIST-Command
    write(socket_fd, "LIST\n", 5);
    
    // Sende Username
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
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

// Sendet READ-Command an Server
void read_message(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== READ MESSAGE ===\n");
    
    // Lese Username
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Lese Message-Nummer
    printf("Message number: ");
    if (fgets(msg_num_str, sizeof(msg_num_str), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Sende READ-Command
    write(socket_fd, "READ\n", 5);
    
    // Sende Username
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
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
        while (readline(socket_fd, buffer, sizeof(buffer)) > 0 && lines_read < 1000) {
            printf("%s", buffer);
            lines_read++;
            
            if (lines_read >= 4 && strlen(buffer) <= 2) {
                // Kurze Wartezeit für weitere Daten
                fd_set readfds;
                struct timeval tv;
                FD_ZERO(&readfds);
                FD_SET(socket_fd, &readfds);
                tv.tv_sec = 0;
                tv.tv_usec = 10000; // 10ms Timeout
                
                // Prüfe ob noch Daten kommen
                if (select(socket_fd + 1, &readfds, NULL, NULL, &tv) <= 0) {
                    // Keine weiteren Daten innerhalb 10ms → Ende
                    break;
                }
            }
        }
        
        printf("--- End of Message ---\n");
    } else {
        printf("Error: Message not found or invalid request\n");
    }
}

// Sendet DEL-Command an Server
void delete_message(int socket_fd) {
    char username[MAX_USERNAME + 1];
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n=== DELETE MESSAGE ===\n");
    
    // Lese Username
    printf("Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    username[strcspn(username, "\n")] = 0;
    
    // Lese Message-Nummer
    printf("Message number: ");
    if (fgets(msg_num_str, sizeof(msg_num_str), stdin) == NULL) {
        printf("Error reading input\n");
        return;
    }
    msg_num_str[strcspn(msg_num_str, "\n")] = 0;
    
    // Sende DEL-Command
    write(socket_fd, "DEL\n", 4);
    
    // Sende Username
    write(socket_fd, username, strlen(username));
    write(socket_fd, "\n", 1);
    
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
    printf("\n===== TW-Mailer Client Menu =====\n");
    printf("1. SEND - Send a message\n");
    printf("2. LIST - List messages for a user\n");
    printf("3. READ - Read a specific message\n");
    printf("4. DEL  - Delete a message\n");
    printf("5. QUIT - Exit the client\n");
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
    
    printf("Connected to TW-Mailer server at %s:%d\n", server_ip, server_port);
    
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
                send_message(socket_fd);
                break;
            case 2:
                list_messages(socket_fd);
                break;
            case 3:
                read_message(socket_fd);
                break;
            case 4:
                delete_message(socket_fd);
                break;
            case 5:
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
