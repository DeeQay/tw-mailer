# TW-Mailer Pro - Protocol

## 1. Projektübersicht

TW-Mailer Pro ist ein Mail-System bestehend aus einem C-Server und C-Client. Benutzer können sich via LDAP authentifizieren und danach Nachrichten senden, lesen, auflisten und löschen.

---

## 2. Server-Architektur

### 2.1 Concurrent Server mit fork()
Der Server erstellt für jede eingehende Verbindung einen Child-Prozess mit `fork()`. Der Parent-Prozess wartet weiter auf neue Verbindungen, während der Child den Client bedient. Verhinderung von Zombie-Prozessen durch `waitpid(-1, NULL, WNOHANG)`.

### 2.2 Session-Management
Jede Client-Verbindung hat eine eigene Session-Struktur:
```c
typedef struct {
    int authenticated; // 0 = !eingeloggt, 1 = eingeloggt
    char username[256]; // LDAP-Username
} Session;
```
Nach erfolgreichem LOGIN wird `authenticated = 1` gesetzt und der Username gespeichert. Alle weiteren Befehle (außer QUIT) prüfen diesen Status.

### 2.3 LDAP-Authentifizierung
- Server: `ldap.technikum-wien.at:389`
- Bind-DN: `uid=<username>,ou=people,dc=technikum-wien,dc=at`
- Timeout: 5 Sekunden
- Bei Verbindungsfehler wird `ERR LDAP\n` zurückgegeben

### 2.4 Blacklist-System
Nach 3 fehlgeschlagenen Login-Versuchen wird die IP für 60 Sekunden gesperrt.

**Speicherung** (`blacklist.dat`):
```
<IP> <Versuche> <Blockiert_bis_timestamp>
127.0.0.1 3 1732982400
```

**Synchronisation**: `flock(LOCK_EX)` auf `.blacklist.lock` verhindert Race Conditions bei gleichzeitigen Schreibzugriffen.

### 2.5 Mail-Spool Struktur
```
<spool-dir>/
├── blacklist.dat
├── .blacklist.lock
├── <username1>/
│   ├── .username1.lock
│   ├── 1 # Erste Nachricht
│   ├── 2 # Zweite Nachricht
│   └── ...
└── <username2>/
    └── ...
```

**Nachrichtenformat** (Dateiinhalt):
```
Sender: <technikum-wien-username>
Receiver:
Subject:
Message:
```

---

## 3. Client-Architektur

- Menügesteuertes Terminal-Interface
- Zeigt aktuellen Login-Status an
- Passwort-Eingabe ohne Echo

---

## 4. Synchronisation

**User-Inbox Lock** (`.username.lock`):
- SEND: Exklusives Lock bevor neue Nachricht geschrieben wird
- DEL: Exklusives Lock bevor Nachricht gelöscht wird
- LIST/READ: Kein Lock nötig (nur Lesezugriff)

```c
int lock_fd = open(".user.lock", O_CREAT | O_RDWR, 0600);
flock(lock_fd, LOCK_EX); // Blockiert bis Lock verfügbar
// ...
flock(lock_fd, LOCK_UN); // Unlock: Lock freigeben
close(lock_fd);
```

---

## 5. Protokoll-Spezifikation

Alle Befehle und Parameter sind durch `\n` getrennt.

## 5.1 LOGIN
**Client → Server**
```
LOGIN <username> <password>
```
**Server → Client:**
```
OK

oder ->

ERR

oder ->

ERR LDAP

oder ->

BLOCKED <sec>
```

---

## 5.2 SEND
Sender kommt aus der Session.

**Client → Server**
```
SEND <receiver> <subject> <message-zeile-1> <message-zeile-2> ...
.
```

**Server → Client**
```
OK

oder ->

ERR
```

---

## 5.3 LIST
Username kommt aus der Session.

**Client → Server**
```
LIST
```

**Server → Client**
```
<anzahl>
<subject-1>
<subject-2>
...
```

---

## 5.4 READ

**Client → Server**

```
READ
<message-nummer>
```

**Server → Client**

```
OK
Sender: <sender>
Receiver: <receiver>
Subject: <subject>
Message:
<inhalt>

oder ->

ERR
```

---

## 5.5 DEL

**Client → Server**

```
DEL
<message-nummer>
```

**Server → Client**

```
OK

oder ->

ERR
```

---

## 5.6 QUIT

**Client → Server**

```
QUIT
```

Verbindung wird geschlossen.