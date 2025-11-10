# TW-Mailer Test-Anleitung

## Build

```bash
make all
```

## Server starten

```bash
./twmailer-server 6543 mailspool
```

## Client starten (in anderem Terminal)

```bash
./twmailer-client 127.0.0.1 6543
```

## Test-Workflow

### 1. Nachricht SENDEN
```
Option: 1
Sender: alice
Receiver: bob
Subject: Test-Nachricht
Message:
Hallo Bob!
Das ist ein Test.
.
```

### 2. Nachrichten AUFLISTEN
```
Option: 2
Username: bob
```

### 3. Nachricht LESEN
```
Option: 3
Username: bob
Message number: 1
```

### 4. Nachricht LÖSCHEN
```
Option: 4
Username: bob
Message number: 1
```

### 5. Erneut AUFLISTEN (Löschung verifizieren)
```
Option: 2
Username: bob
```

### 6. BEENDEN
```
Option: 5
```

## Schnelltest mit netcat

### Nachricht senden
```bash
echo -e "SEND\nalice\nbob\nTest\nHallo Welt\n.\nQUIT" | nc localhost 6543
```

### Nachrichten auflisten
```bash
echo -e "LIST\nbob\nQUIT" | nc localhost 6543
```

### Nachricht #1 lesen
```bash
echo -e "READ\nbob\n1\nQUIT" | nc localhost 6543
```

### Nachricht #1 löschen
```bash
echo -e "DEL\nbob\n1\nQUIT" | nc localhost 6543
```

## Edge-Case Tests

### Ungültiger Username (zu lang)
```bash
echo -e "SEND\nalice123456789\nbob\nTest\nHallo\n.\nQUIT" | nc localhost 6543
# Erwartet: ERR
```

### Ungültiger Username (Großbuchstaben)
```bash
echo -e "SEND\nAlice\nbob\nTest\nHallo\n.\nQUIT" | nc localhost 6543
# Erwartet: ERR
```

### Ungültiger Betreff (zu lang, >80 Zeichen)
```bash
echo -e "SEND\nalice\nbob\n$(python3 -c "print('A'*81)")\nTest\n.\nQUIT" | nc localhost 6543
# Erwartet: ERR
```

### Nicht-existierende Nachricht lesen
```bash
echo -e "READ\nbob\n99\nQUIT" | nc localhost 6543
# Erwartet: ERR
```

## Aufräumen

```bash
make clean
rm -rf mailspool
```
