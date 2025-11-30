#include "input.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "common.h"

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
