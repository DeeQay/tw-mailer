# TW-Mailer Client - Code-Erklärung

## Überblick
Der TW-Mailer Client ist ein **interaktives Kommandozeilen-Programm**, das mit dem TW-Mailer Server kommuniziert. Es bietet ein Menu zum Senden, Lesen, Auflisten und Löschen von Nachrichten.

## Start des Clients
```bash
./twmailer-client <server-ip> <port>
```
- **Server-IP**: IP-Adresse des Servers (z.B. 127.0.0.1 für localhost)
- **Port**: Port des Servers (z.B. 6543)

---

## Hauptstruktur

### Verbindungsaufbau
```c
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
```

**Was passiert hier?**
1. Socket wird erstellt
2. Server-Adresse wird konfiguriert (`inet_pton` konvertiert IP-String zu Binärformat)
3. TCP-Verbindung wird hergestellt mit `connect()`

---

### Main-Loop
```c
while (1) {
    display_menu();
    // User wählt Option
    switch (option) {
        case 1: send_message(socket_fd); break;
        case 2: list_messages(socket_fd); break;
        case 3: read_message(socket_fd); break;
        case 4: delete_message(socket_fd); break;
        case 5: /* QUIT */ break;
    }
}
```

Der Client läuft in einer **Endlosschleife** und zeigt das Menu immer wieder, bis der User QUIT wählt.

---

## Die 4 Funktionen

### 1. SEND - Nachricht senden
```c
void send_message(int socket_fd)
```

**Ablauf:**
1. **Eingaben vom User holen:**
   ```c
   printf("Sender: ");
   fgets(sender, sizeof(sender), stdin);
   sender[strcspn(sender, "\n")] = 0;  // Entferne \n
   ```

2. **Protokoll an Server senden:**
   ```
   SEND\n
   alice\n
   bob\n
   Hello\n
   This is a message.\n
   .\n
   ```

3. **Server-Antwort lesen:**
   ```c
   readline(socket_fd, response, sizeof(response));
   if (strcmp(response, "OK") == 0) {
       printf("Message sent successfully!\n");
   }
   ```

**Wichtig: Der Punkt als Ende-Marker**
```c
while (1) {
    fgets(line, sizeof(line), stdin);
    write(socket_fd, line, strlen(line));
    if (strcmp(line, ".\n") == 0) {
        break;  // Fertig!
    }
}
```
Der User muss eine Zeile mit nur einem Punkt eingeben, um die Nachricht zu beenden.

---

### 2. LIST - Nachrichten auflisten
```c
void list_messages(int socket_fd)
```

**Ablauf:**
1. Username vom User holen
2. **Protokoll an Server:**
   ```
   LIST\n
   alice\n
   ```
3. **Server antwortet:**
   ```
   2\n
   Hello from Bob\n
   Meeting tomorrow\n
   ```
   - Erste Zeile: Anzahl der Nachrichten
   - Dann: Alle Subjects

4. **Ausgabe formatieren:**
   ```c
   for (int i = 0; i < count; i++) {
       readline(socket_fd, buffer, sizeof(buffer));
       printf("%d. %s\n", i + 1, buffer);
   }
   ```

**Ergebnis für User:**
```
Number of messages: 2

Subjects:
1. Hello from Bob
2. Meeting tomorrow
```

---

### 3. READ - Nachricht lesen
```c
void read_message(int socket_fd)
```

**Ablauf:**
1. Username und Message-Nummer vom User holen
2. **Protokoll an Server:**
   ```
   READ\n
   alice\n
   2\n
   ```
3. **Server antwortet:**
   ```
   OK\n
   Sender: bob\n
   Receiver: alice\n
   Subject: Meeting tomorrow\n
   Message:\n
   Don't forget the meeting!\n
   ```

4. **Kompletten Inhalt ausgeben:**
   ```c
   while (readline(socket_fd, buffer, sizeof(buffer)) > 0) {
       printf("%s", buffer);
   }
   ```

**Besonderheit:**
```c
if (strcmp(buffer, "OK") == 0) {
    // Erfolg - lese Nachricht
} else {
    printf("Error: Message not found\n");
}
```
Der Client prüft erst die Antwort, bevor er die Nachricht liest.

---

### 4. DEL - Nachricht löschen
```c
void delete_message(int socket_fd)
```

**Ablauf:**
1. Username und Message-Nummer vom User holen
2. **Protokoll an Server:**
   ```
   DEL\n
   alice\n
   2\n
   ```
3. **Server antwortet:**
   ```
   OK\n
   ```
   oder
   ```
   ERR\n
   ```

4. **Feedback an User:**
   ```c
   if (strcmp(buffer, "OK") == 0) {
       printf("Message deleted successfully!\n");
   } else {
       printf("Error deleting message\n");
   }
   ```

---

## Hilfsfunktionen

### `readline()` - Zeilenweises Lesen
```c
int readline(int fd, char *buffer, int max_len)
```

**Identisch zum Server!** Liest Zeichen für Zeichen vom Socket bis `\n` gefunden wird.

**Warum wichtig?**
- TCP-Daten kommen nicht immer in ganzen Zeilen an
- Ein `read()` könnte nur "Hel" statt "Hello\n" liefern
- `readline()` sammelt Zeichen bis die Zeile komplett ist

---

### `display_menu()` - Menu anzeigen
```c
void display_menu()
```

Zeigt einfach das User-Menu:
```
===== TW-Mailer Client Menu =====
1. SEND - Send a message
2. LIST - List messages for a user
3. READ - Read a specific message
4. DEL  - Delete a message
5. QUIT - Exit the client
================================
```

---

## String-Verarbeitung

### Newline entfernen
```c
sender[strcspn(sender, "\n")] = 0;
```

**Was macht das?**
- `fgets()` liest auch das `\n` am Ende
- `strcspn(sender, "\n")` findet die Position des `\n`
- Dort wird ein `\0` (String-Ende) gesetzt
- Resultat: Das `\n` wird "abgeschnitten"

**Beispiel:**
```
Vorher:  "alice\n\0"
Nachher: "alice\0\n"  (aber \0 beendet den String)
```

---

## QUIT-Funktionalität

```c
case 5:
    printf("Sending QUIT command...\n");
    write(socket_fd, "QUIT\n", 5);
    printf("Goodbye!\n");
    close(socket_fd);
    return EXIT_SUCCESS;
```

**Ablauf:**
1. Sende `QUIT\n` an Server
2. Server beendet die Verbindung
3. Client schließt Socket und beendet sich

**Wichtig:** Der Client wartet **nicht** auf eine Antwort vom Server bei QUIT.

---

## Fehlerbehandlung

### Bei Verbindungsproblemen
```c
if (connect(socket_fd, ...) == -1) {
    perror("connect");
    close(socket_fd);
    return EXIT_FAILURE;
}
```

### Bei Lesefehler
```c
if (readline(socket_fd, buffer, sizeof(buffer)) <= 0) {
    printf("Error reading response from server\n");
    return;
}
```

**Was passiert bei Fehler?**
- Fehlermeldung wird ausgegeben
- Funktion kehrt zurück zum Menu
- Client läuft weiter (stürzt nicht ab)

---

## User Experience Details

### Eingabeaufforderungen
Der Client gibt dem User klare Hinweise:
```c
printf("Sender (max %d chars, a-z, 0-9): ", MAX_USERNAME);
```
So weiß der User genau, was erlaubt ist.

### Feedback
Nach jeder Aktion gibt's eine Rückmeldung:
- ✅ "Message sent successfully!"
- ❌ "Error sending message"
- ℹ️ "Number of messages: 2"

---

## Kommunikationsprotokoll

### Alle Nachrichten enden mit `\n`
```c
write(socket_fd, "SEND\n", 5);
write(socket_fd, sender, strlen(sender));
write(socket_fd, "\n", 1);
```

Jede Zeile wird mit einem Newline abgeschlossen, damit der Server weiß, wo eine Information endet.

### Client wartet auf Antwort
Nach jedem Command wartet der Client auf die Server-Antwort, bevor er weiter macht:
```c
write(socket_fd, "LIST\n", 5);
// ... sende weitere Daten ...
readline(socket_fd, buffer, ...);  // Warte auf Antwort
```

---

## Limitierungen

1. **Keine Eingabe-Validierung** - Client schickt alles zum Server, Server prüft
2. **Keine automatische Wiederverbindung** - Bei Verbindungsabbruch muss Client neu gestartet werden
3. **Blocking I/O** - Client wartet immer auf Server-Antwort
4. **Keine Historie** - Kein Zurückgehen zu vorherigen Befehlen
5. **Maximale Längen** müssen vom User beachtet werden

---

## Unterschied zu Server

| Aspekt | Server | Client |
|--------|--------|--------|
| Rolle | Wartet auf Verbindungen | Initiiert Verbindung |
| Loop | Endlos (`while(1)`) | Menu-gesteuert |
| Speicherung | Schreibt Dateien | Nur Anzeige |
| Validierung | Prüft alle Eingaben | Minimal |
| Fehlerbehandlung | Sendet ERR/OK | Zeigt Fehler an |
