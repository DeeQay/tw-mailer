#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME 8
#define MAX_SUBJECT 80
#define MAX_LDAP_USERNAME 256
#define MAX_PASSWORD 256
#define MAX_PATH 1024

#define LDAP_HOST "ldap.technikum-wien.at"
#define LDAP_PORT 389
#define LDAP_SEARCH_BASE "dc=technikum-wien,dc=at"

#define MAX_LOGIN_ATTEMPTS 3
#define BLACKLIST_DURATION 60
#define BLACKLIST_FILE "blacklist.dat"

// Globale Variable für das Mail-Spool-Verzeichnis
extern char mail_spool_dir[256];

// Session-Struktur für authentifizierten Benutzer
typedef struct {
    int authenticated;
    char username[MAX_LDAP_USERNAME];
} Session;

// Blacklist-Eintrag Struktur
typedef struct {
    char ip[16];
    int attempts;
    time_t block_until;
} BlacklistEntry;

int readline(int fd, char *buffer, int max_len);

#endif // SERVER_COMMON_H
