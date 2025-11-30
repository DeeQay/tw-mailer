#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "common.h"
#include "input.h"

// Sendet LOGIN-Command an Server
void login(int socket_fd) {
    char username[MAX_LDAP_USERNAME + 1];
    char password[MAX_PASSWORD + 1];
    char response[32];
    
    printf("\n--- LOGIN ---\n");
    
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
        } else if (strncmp(response, "BLOCKED ", 8) == 0) {
            int seconds = atoi(response + 8);
            printf("Your IP is blocked! Try again in %d seconds.\n", seconds);
        } else if (strcmp(response, "ERR LDAP") == 0) {
            printf("LDAP server unreachable! Please check your VPN connection.\n");
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

    printf("\n SEND MESSAGE \n");
    
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
    
    printf("\n LIST MESSAGES \n");
    
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

// Sendet READ-Command an Server, username aus Session
void read_message(int socket_fd) {
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n READ MESSAGE \n");
    
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

            // Warte auf Daten vom Socket oder Tastatureingabe
            if (select(socket_fd + 1, &readfds, NULL, NULL, NULL) < 0) { 
                break;
            }

            // Überprüfe, ob Tastatureingabe vorliegt
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF); // Eingabepuffer leeren
                break;
            }

            // Überprüfe, ob Daten vom Socket vorliegen
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

// Sendet DEL-Command an Server
void delete_message(int socket_fd) {
    char msg_num_str[16];
    char buffer[BUFFER_SIZE];
    
    printf("\n DELETE MESSAGE \n");
    
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
    
    // Sende DEL-Command 
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
