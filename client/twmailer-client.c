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
#include <termios.h>
#include "common.h"
#include "commands.h"
#include "input.h"


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
