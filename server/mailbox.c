#include "mailbox.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

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