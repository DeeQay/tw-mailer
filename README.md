# TW-Mailer - Basic Version

## Beschreibung

TW-Mailer ist eine socket-basierte Client-Server-Anwendung in C, die ein einfaches internes Mail-Server-System implementiert. Die Anwendung ermöglicht es Benutzern, Nachrichten über ein proprietäres Klartext-Protokoll zu senden, aufzulisten, zu lesen und zu löschen.

## Architektur

### Server-Architektur
- **Typ**: Iterativer Server (single-threaded, behandelt einen Client zur Zeit)
- **Kommunikation**: Stream Socket (TCP) mit IPv4
- **Port**: Konfigurierbar über Kommandozeilen-Argument
- **Nachrichtenspeicherung**: Dateibasierte Persistenz in einem Mail-Spool-Verzeichnis
  - Jeder Benutzer hat sein eigenes Inbox-Verzeichnis: `<mail-spool-dir>/<username>/`
  - Jede Nachricht wird als separate nummerierte Datei gespeichert (1, 2, 3, ...)
  - Nachrichtendateien enthalten Sender, Empfänger, Betreff und Nachrichteninhalt

### Client-Architektur
- **Typ**: Interaktiver Kommandozeilen-Client
- **Kommunikation**: TCP-Socket-Verbindung zum Server
- **Interface**: Menügesteuertes Interface mit 5 Optionen (SEND, LIST, READ, DEL, QUIT)
- **Input-Validierung**: Client-seitige Validierung für Username-Format und Betreff-Länge

### Protokoll-Design
Die Anwendung verwendet ein eigenes zeilenorientiertes Protokoll:
- Commands und Parameter sind durch Newlines (`\n`) getrennt
- Mehrzeiliger Nachrichteninhalt wird durch eine Zeile mit nur einem Punkt (`.`) terminiert
- Server-Antworten sind entweder `OK\n`, `ERR\n`, oder Daten gefolgt von Newline

## Verwendete Technologien

- **Programmiersprache**: C (C11 Standard)
- **Netzwerk-Programmierung**: POSIX Sockets API
  - `socket()` - Erstellt Kommunikations-Endpunkte
  - `bind()` - Bindet Socket an Adresse
  - `listen()` - Lauscht auf Verbindungen
  - `accept()` - Akzeptiert eingehende Verbindungen
  - `connect()` - Verbindet zum Server
  - `read()`/`write()` - Datenübertragung
- **File I/O**: Standard C-Dateioperationen (`fopen`, `fgets`, `fprintf`, etc.)
- **Verzeichnis-Management**: POSIX APIs (`mkdir`, `stat`, `unlink`)
- **Compiler**: GCC mit Warning-Flags für Code-Qualität

## Entwicklungsstrategie

### Phase 1: Protokoll-Design
- Definition der Command-Struktur und Nachrichtenformat
- Festlegung der Validierungsregeln (Username: max 8 Zeichen, alphanumerisch; Betreff: max 80 Zeichen)

### Phase 2: Server-Implementierung
1. Socket-Erstellung und Binding
2. Verbindungshandling (iteratives Modell)
3. Command-Parsing mit `readline()`-Funktion
4. Implementierung der Command-Handler:
   - `SEND`: Erstellt User-Inbox, speichert Nachricht in Datei
   - `LIST`: Liest Inbox-Verzeichnis, extrahiert Betreffe
   - `READ`: Ruft vollständige Nachricht ab und sendet sie
   - `DEL`: Entfernt Nachrichtendatei
   - `QUIT`: Schließt Verbindung
5. Fehlerbehandlung und Validierung

### Phase 3: Client-Implementierung
1. Socket-Erstellung und Verbindung
2. Interaktives Menü-System
3. Benutzer-Input-Handling für jeden Command
4. Protokoll-konforme Nachrichtenformatierung
5. Response-Parsing und Anzeige

### Phase 4: Testing und Verfeinerung
1. Memory-Leak-Testing (valgrind)
2. Input-Validierungs-Tests
3. Fehlerfall-Handling
4. Multi-Message-Testing

## Notwendige Anpassungen

Während der Entwicklung wurden folgende Anpassungen vorgenommen:

1. **Buffer-Management**: Sorgfältiges Buffer-Size-Management zur Verhinderung von Overflows
   - Verwendung von `strncpy()` und größenbegrenzten `fgets()`-Aufrufen
   - Bounds-Checking für alle Benutzereingaben

2. **File-Storage-Strategie**: Eine-Datei-pro-Nachricht-Ansatz gewählt
   - Einfachere Implementierung
   - Einfacheres Löschen von Nachrichten
   - Bessere Fehlertoleranz (beschädigte Datei betrifft nur eine Nachricht)

3. **Nachrichten-Nummerierung**: Sequenzielle Integer-Benennung für Nachrichtendateien
   - Einfach zu implementieren
   - Leicht nächste verfügbare Nummer zu finden
   - Direkte Zuordnung von LIST-Output zu READ/DEL-Commands

4. **Fehlerbehandlung**: Umfassendes Error-Checking
   - Socket-Operations-Fehler
   - File-I/O-Fehler
   - Verzeichnis-Erstellungs-Fehler
   - Input-Validierungs-Fehler

5. **Username-Validierung**: Strikte Validierung von Usernames
   - Nur Kleinbuchstaben (a-z) und Ziffern (0-9)
   - Maximum 8 Zeichen
   - Verhindert Directory-Traversal-Angriffe

## Projekt bauen

```bash
# Kompiliere Client und Server
make all

# Bereinige Build-Artefakte
make clean
```

## Verwendung

### Server
```bash
./twmailer-server <port> <mail-spool-directory>
```

Beispiel:
```bash
./twmailer-server 6543 ./mailspool
```

### Client
```bash
./twmailer-client <ip> <port>
```

Beispiel:
```bash
./twmailer-client 127.0.0.1 6543
```