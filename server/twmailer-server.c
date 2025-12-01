// Usage: ./twmailer-server <port> <mail-spool-directory>

#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "blacklist.h"
#include "auth.h"
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

// Signal Handler für SIGCHLD - verhindert Zombie-Prozesse
void sigchld_handler(int sig) {
    (void)sig; // braucht man 
    // Reape alle beendeten Kindprozesse
    while (waitpid(-1, NULL, WNOHANG) > 0);
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
            //  CHILD PROCESS
            // braucht den Server-Socket nicht
            close(server_socket);
            
            // Behandle Client-Anfragen (mit Client-IP für Blacklist)
            handle_client(client_socket, client_ip);
            
            // Schließe Client-Verbindung
            close(client_socket);
            printf("Child process %d: Client connection closed\n", getpid());
            
            // Kindprozess beenden
            exit(EXIT_SUCCESS);
        } else {
            // PARENT PROCESS
            // braucht den Client-Socket nicht
            close(client_socket);
            
            printf("Spawned child process %d for client\n", pid);
            // Weiter mit accept() für nächsten Client
        }
    }
    
    // Server-Socket schließen (wird nie erreicht)
    close(server_socket);
    return EXIT_SUCCESS;
}
