#include "blacklist.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

// Blacklist-Funktionen hierher verschieben
// Erwirbt ein Lock auf die Blacklist-Datei
int acquire_blacklist_lock() {
    char lock_file[MAX_PATH];
    snprintf(lock_file, sizeof(lock_file), "%s/.blacklist.lock", mail_spool_dir);
    
    int lock_fd = open(lock_file, O_CREAT | O_RDWR, 0600);
    if (lock_fd == -1) {
        perror("open blacklist lock file");
        return -1;
    }
    
    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock blacklist");
        close(lock_fd);
        return -1;
    }
    
    return lock_fd;
}

// Gibt das Blacklist-Lock frei
void release_blacklist_lock(int lock_fd) {
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
}

// Lädt Blacklist-Eintrag für eine IP
// Rückgabe: 1 wenn gefunden, 0 wenn nicht gefunden
int load_blacklist_entry(const char *ip, BlacklistEntry *entry) {
    char blacklist_path[MAX_PATH];
    snprintf(blacklist_path, sizeof(blacklist_path), "%s/%s", mail_spool_dir, BLACKLIST_FILE);
    
    FILE *fp = fopen(blacklist_path, "r");
    if (fp == NULL) {
        return 0; // Datei existiert nicht
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char stored_ip[INET_ADDRSTRLEN];
        int attempts;
        long block_until;
        
        if (sscanf(line, "%15s %d %ld", stored_ip, &attempts, &block_until) == 3) {
            if (strcmp(stored_ip, ip) == 0) {
                strncpy(entry->ip, stored_ip, INET_ADDRSTRLEN - 1);
                entry->ip[INET_ADDRSTRLEN - 1] = '\0';
                entry->attempts = attempts;
                entry->block_until = (time_t)block_until;
                fclose(fp);
                return 1;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

// Speichert/Aktualisiert Blacklist-Eintrag für eine IP
void save_blacklist_entry(const char *ip, int attempts, time_t block_until) {
    char blacklist_path[MAX_PATH];
    char temp_path[MAX_PATH];
    snprintf(blacklist_path, sizeof(blacklist_path), "%s/%s", mail_spool_dir, BLACKLIST_FILE);
    snprintf(temp_path, sizeof(temp_path), "%s/%s.tmp", mail_spool_dir, BLACKLIST_FILE);
    
    FILE *fp_in = fopen(blacklist_path, "r");
    FILE *fp_out = fopen(temp_path, "w");
    
    if (fp_out == NULL) {
        if (fp_in) fclose(fp_in);
        return;
    }
    
    int found = 0;
    
    if (fp_in != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp_in) != NULL) {
            char stored_ip[INET_ADDRSTRLEN];
            int stored_attempts;
            long stored_block;
            
            if (sscanf(line, "%15s %d %ld", stored_ip, &stored_attempts, &stored_block) == 3) {
                if (strcmp(stored_ip, ip) == 0) {
                    // Aktualisiere diesen Eintrag
                    fprintf(fp_out, "%s %d %ld\n", ip, attempts, (long)block_until);
                    found = 1;
                } else {
                    // Kopiere unverändert
                    fprintf(fp_out, "%s", line);
                }
            }
        }
        fclose(fp_in);
    }
    
    // Wenn IP nicht gefunden, füge neuen Eintrag hinzu
    if (!found) {
        fprintf(fp_out, "%s %d %ld\n", ip, attempts, (long)block_until);
    }
    
    fclose(fp_out);
    
    // Ersetze alte Datei durch neue
    rename(temp_path, blacklist_path);
}

// Prüft ob IP aktuell gesperrt ist
// Rückgabe: Verbleibende Sekunden wenn gesperrt, 0 wenn nicht gesperrt
int get_blacklist_remaining(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return 0;
    
    BlacklistEntry entry;
    int remaining = 0;
    
    if (load_blacklist_entry(ip, &entry)) {
        time_t now = time(NULL);
        if (entry.block_until > now) {
            remaining = (int)(entry.block_until - now);
        }
    }
    
    release_blacklist_lock(lock_fd);
    return remaining;
}

// Registriert fehlgeschlagenen Login-Versuch
// Rückgabe: Verbleibende Versuche (0 = jetzt gesperrt)
int register_failed_login(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return MAX_LOGIN_ATTEMPTS;
    
    BlacklistEntry entry;
    int attempts = 1;
    time_t block_until = 0;
    
    if (load_blacklist_entry(ip, &entry)) {
        time_t now = time(NULL);
        
        // Wenn Sperre abgelaufen, reset Attempts
        if (entry.block_until > 0 && entry.block_until <= now) {
            attempts = 1;
        } else {
            attempts = entry.attempts + 1;
        }
    }
    
    // Bei 3 Versuchen: Sperre für 1 Minute
    if (attempts >= MAX_LOGIN_ATTEMPTS) {
        block_until = time(NULL) + BLACKLIST_DURATION;
        printf("IP %s blocked for %d seconds\n", ip, BLACKLIST_DURATION);
    }
    
    save_blacklist_entry(ip, attempts, block_until);
    release_blacklist_lock(lock_fd);
    
    return MAX_LOGIN_ATTEMPTS - attempts;
}

// Registriert erfolgreichen Login (löscht Blacklist-Eintrag)
void register_successful_login(const char *ip) {
    int lock_fd = acquire_blacklist_lock();
    if (lock_fd == -1) return;
    
    save_blacklist_entry(ip, 0, 0); // Eintrag löschen
    release_blacklist_lock(lock_fd);
}