#ifndef CLIENT_COMMON_H
#define CCLIENT_COMMON_H
#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80
#define MAX_MESSAGE_SIZE 8192
#define MAX_LDAP_USERNAME 256
#define MAX_PASSWORD 256

// Globale Variable für Session-Status
extern int is_logged_in;

int readline(int fd, char *buffer, int max_len);

#endif // CLIENT_COMMON_H