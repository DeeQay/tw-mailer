#include "common.h"
#include <unistd.h>

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