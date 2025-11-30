# TW-Mailer - Pro Version

## Beschreibung

TW-Mailer ist eine socket-basierte Client-Server-Anwendung in C, die ein internes Mail-Server-System mit LDAP-Authentifizierung implementiert. Die Anwendung ermöglicht es authentifizierten Benutzern, Nachrichten über ein proprietäres Klartext-Protokoll zu senden, aufzulisten, zu lesen und zu löschen.

## Architektur

### Server-Architektur
- **Typ**: Concurrent Server mit fork() (multi-process, behandelt mehrere Clients parallel)
- **Kommunikation**: Stream Socket (TCP) mit IPv4
- **Port**: Konfigurierbar über Kommandozeilen-Argument
- **Authentifizierung**: LDAP-basierte Benutzeranmeldung gegen ldap.technikum.wien.at
- **Session-Management**: Pro Verbindung wird eine Session mit Username verwaltet
- **Blacklist-System**: IP-Sperre nach 3 fehlgeschlagenen Login-Versuchen (1 Minute)
- **Nachrichtenspeicherung**: Dateibasierte Persistenz in einem Mail-Spool-Verzeichnis
  - Jeder Benutzer hat sein eigenes Inbox-Verzeichnis: `<mail-spool-dir>/<username>/`
  - Jede Nachricht wird als separate nummerierte Datei gespeichert (1, 2, 3, ...)
  - Nachrichtendateien enthalten Sender, Empfänger, Betreff und Nachrichteninhalt
- **Synchronisation**: File-Locking (flock) für kritische Sektionen

### Client-Architektur
- **Typ**: Interaktiver Kommandozeilen-Client
- **Kommunikation**: TCP-Socket-Verbindung zum Server
- **Interface**: Menügesteuertes Interface mit 6 Optionen (LOGIN, SEND, LIST, READ, DEL, QUIT)
- **Login-Status**: Anzeige des aktuellen Authentifizierungsstatus
- **Sichere Passworteingabe**: Passwort wird ohne Echo eingegeben

### Protokoll-Design
Die Anwendung verwendet ein eigenes zeilenorientiertes Protokoll:
- Commands und Parameter sind durch Newlines (`\n`) getrennt
- LOGIN erfordert LDAP-Username und Passwort
- SEND/LIST/READ/DEL benötigen vorherige Authentifizierung
- QUIT ist immer erlaubt (auch ohne Login)
- Mehrzeiliger Nachrichteninhalt wird durch eine Zeile mit nur einem Punkt (`.`) terminiert
- Server-Antworten sind entweder `OK\n`, `ERR\n`, oder Daten gefolgt von Newline

## Verwendete Technologien

- **Programmiersprache**: C
- **LDAP**: OpenLDAP C-API (libldap)
  - Authentifizierung gegen ldap.technikum.wien.at:389
- **Netzwerk-Programmierung**: POSIX Sockets API
  - `socket()` - Erstellt Kommunikations-Endpunkte
  - `bind()` - Bindet Socket an Adresse
  - `listen()` - Lauscht auf Verbindungen
  - `accept()` - Akzeptiert eingehende Verbindungen
  - `connect()` - Verbindet zum Server
  - `read()`/`write()` - Datenübertragung
  - `fork()` - Erstellt Kindprozesse für parallele Client-Behandlung
- **Synchronisation**: POSIX File-Locking (`flock`)
- **File I/O**: Standard C-Dateioperationen (`fopen`, `fgets`, `fprintf`, etc.)
- **Verzeichnis-Management**: POSIX APIs (`mkdir`, `stat`, `unlink`)
- **Compiler**: GCC mit Warning-Flags für Code-Qualität

## Abhängigkeiten

```bash
# Ubuntu/Debian
sudo apt install libldap2-dev
```

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

## Protokoll-Befehle

| Befehl | Beschreibung | Authentifizierung erforderlich |
|--------|--------------|-------------------------------|
| LOGIN  | LDAP-Authentifizierung | Nein |
| SEND   | Nachricht senden | Ja |
| LIST   | Eigene Nachrichten auflisten | Ja |
| READ   | Nachricht lesen | Ja |
| DEL    | Nachricht löschen | Ja |
| QUIT   | Verbindung beenden | Nein |