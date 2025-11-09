# TW-Mailer Server - Code-Erklärung

## Überblick
Der TW-Mailer Server ist ein einfacher Mail-Server, der über TCP-Sockets mit Clients kommuniziert. Er speichert Nachrichten in einem Dateisystem (Mail-Spool-Directory).

## Start des Servers
```bash
./twmailer-server <port> <mail-spool-directory>
```
- **Port**: Auf welchem Port der Server hört (z.B. 6543)
- **Mail-Spool-Directory**: Verzeichnis zum Speichern der Nachrichten (z.B. ./mailspool)

---

## Wichtige Konzepte

### Socket-Programmierung
Der Server nutzt **TCP-Sockets** für die Netzwerkkommunikation:

```c
int server_socket = socket(AF_INET, SOCK_STREAM, 0);
```
- `AF_INET`: IPv4-Protokoll
- `SOCK_STREAM`: TCP (zuverlässige Verbindung)

### Iterativer Server
Der Server ist **iterativ** (nicht parallel):
```c
while (1) {
    int client_socket = accept(server_socket, ...);
    handle_client(client_socket);  // Blockiert bis Client fertig ist
    close(client_socket);
}
```
Das bedeutet: Der Server bearbeitet **immer nur einen Client** zur Zeit. Während ein Client bedient wird, müssen andere warten.

---

## Kernfunktionen

### 1. `readline()` - Zeilenweises Lesen
```c
int readline(int fd, char *buffer, int max_len)
```
Liest **Zeichen für Zeichen** vom Socket bis ein `\n` gefunden wird.

**Warum Zeichen für Zeichen?**
- TCP garantiert keine Nachrichtengrenzen
- Ein `read()` könnte nur einen Teil der Zeile liefern
- Diese Funktion stellt sicher, dass wir eine komplette Zeile bekommen

**Rückgabewerte:**
- Positive Zahl: Anzahl gelesener Zeichen
- 0: Verbindung wurde geschlossen
- -1: Fehler beim Lesen

---

### 2. Username-Validierung
```c
int validate_username(const char *username)
```
Prüft ob der Username gültig ist:
- **Maximal 8 Zeichen** lang
- Nur **Kleinbuchstaben (a-z)** und **Ziffern (0-9)**

**Warum diese Einschränkungen?**
- Verhindert Sicherheitsprobleme (z.B. `../` in Pfaden)
- Macht Dateinamen vorhersehbar

---

### 3. Nachrichtenspeicherung

#### Verzeichnisstruktur
```
mailspool/
  ├── alice/
  │   ├── 1
  │   ├── 2
  │   └── 3
  └── bob/
      └── 1
```
- Jeder User bekommt ein eigenes Verzeichnis
- Nachrichten sind durchnummeriert (1, 2, 3, ...)

#### Dateiformat einer Nachricht
```
Sender: alice
Receiver: bob
Subject: Hallo Bob
Message:
Wie geht es dir?
Das ist eine Testnachricht.
```

---

## Die 4 Kommandos

### SEND - Nachricht senden
```c
void handle_send(int client_socket)
```

**Ablauf:**
1. Lese Sender, Receiver, Subject vom Client
2. **Validiere** alle Eingaben (Username-Format, Subject-Länge)
3. Erstelle Inbox-Verzeichnis für Receiver falls nötig
4. Finde nächste freie Nachrichtennummer
5. Erstelle Datei und schreibe Nachricht
6. Lese Message-Zeilen bis `.\n` kommt
7. Sende `OK` oder `ERR` zurück

**Besonderheit:**
```c
if (strcmp(line, ".\n") == 0) {
    break;  // Ende der Nachricht
}
```
Der Punkt auf einer eigenen Zeile markiert das Ende der Nachricht.

---

### LIST - Nachrichten auflisten
```c
void handle_list(int client_socket)
```

**Ablauf:**
1. Lese Username
2. Durchsuche User-Verzeichnis nach Nachrichten (Dateien 1, 2, 3, ...)
3. Lese aus jeder Datei die **Subject-Zeile**
4. Sende Anzahl und alle Subjects zurück

**Warum zählt der Server so?**
```c
while (num <= 100 && count < 100) {
    snprintf(msg_file, sizeof(msg_file), "%s/%s/%d", 
             mail_spool_dir, username, num);
    FILE *fp = fopen(msg_file, "r");
    if (fp != NULL) { count++; }
    num++;
}
```
Der Server probiert einfach Nummern 1-100 durch und prüft welche Dateien existieren. Das ist einfach, aber nicht sehr effizient.

---

### READ - Nachricht lesen
```c
void handle_read(int client_socket)
```

**Ablauf:**
1. Lese Username und Message-Nummer
2. Validiere Eingaben
3. Öffne entsprechende Datei (`mailspool/username/nummer`)
4. Sende `OK` + **gesamten Dateiinhalt** an Client

**Wichtig:**
```c
while (fgets(line, sizeof(line), fp) != NULL) {
    write(client_socket, line, strlen(line));
}
```
Die komplette Nachricht (mit Metadaten) wird zum Client gesendet.

---

### DEL - Nachricht löschen
```c
void handle_del(int client_socket)
```

**Ablauf:**
1. Lese Username und Message-Nummer
2. Validiere Eingaben
3. Lösche Datei mit `unlink()`
4. Sende `OK` oder `ERR`

**Was passiert mit der Lücke?**
Wenn Nachricht 2 gelöscht wird, bleibt eine Lücke:
```
1  (existiert)
2  (gelöscht - Lücke!)
3  (existiert)
```
Das ist kein Problem, weil `LIST` nur existierende Dateien zählt.

---

## Socket-Optionen

### SO_REUSEADDR
```c
int opt = 1;
setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**Warum wichtig?**
- Normalerweise kann man einen Port nicht sofort nach Server-Neustart nutzen
- Diese Option erlaubt **schnelles Neustarten** des Servers
- Verhindert "Address already in use" Fehler

---

## Sicherheitsaspekte

### Path Traversal Prevention
Durch die Username-Validierung wird verhindert, dass jemand `../secret` als Username verwendet und auf andere Verzeichnisse zugreift.

### Buffer Overflow Protection
```c
char sender[MAX_USERNAME + 2];  // +2 für \n und \0
```
Buffer sind größer als nötig, um Overflows zu vermeiden.

---

## Fehlerbehandlung

Der Server sendet bei Problemen immer `ERR\n`:
- Ungültiger Username
- Nachricht existiert nicht
- Datei kann nicht erstellt werden
- etc.

Bei Erfolg: `OK\n`

---

## Limitierungen

1. **Nur ein Client gleichzeitig** (iterativer Server)
2. **Keine Authentifizierung** - jeder kann jede Mailbox lesen
3. **Maximale Nachrichtengröße** durch `BUFFER_SIZE` limitiert
4. **Ineffiziente Suche** bei LIST (probiert alle Nummern 1-100)
5. **Keine Fehlerbehandlung** bei unvollständigen Commands
