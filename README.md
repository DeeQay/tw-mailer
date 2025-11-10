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

- **Programmiersprache**: C 
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