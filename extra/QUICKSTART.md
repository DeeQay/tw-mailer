# TW-Mailer Schnellstart-Anleitung

## Build-Anweisungen

```bash
make all
```

Zum Aufräumen:
```bash
make clean
```

## Anwendung starten

### 1. Server starten

Terminal öffnen und ausführen:
```bash
./twmailer-server 6543 mailspool
```

Dies startet den Server auf Port 6543 mit Nachrichtenspeicherung im `mailspool`-Verzeichnis.

### 2. Client starten

Weiteres Terminal öffnen und ausführen:
```bash
./twmailer-client 127.0.0.1 6543
```

Dies verbindet zum Server auf localhost (127.0.0.1) an Port 6543.

## Client verwenden

Der Client präsentiert ein Menü mit 5 Optionen:

### 1. SEND - Nachricht senden
- Sender-Username eingeben (max 8 Zeichen, a-z und 0-9)
- Empfänger-Username eingeben
- Betreff eingeben (max 80 Zeichen)
- Nachricht eingeben (mehrere Zeilen)
- Nachricht mit einer Zeile beenden, die nur einen Punkt (.) enthält

Beispiel:
```
Sender: alice
Receiver: bob
Subject: Meeting Tomorrow
Message:
Hi Bob,
Let's meet tomorrow at 10am.
Best regards,
Alice
.
```

### 2. LIST - Nachrichten auflisten
- Username eingeben, für den Nachrichten aufgelistet werden sollen
- Zeigt Anzahl und Betreffe aller Nachrichten

Beispiel:
```
Username: bob
```

### 3. READ - Nachricht lesen
- Username eingeben
- Nachrichtennummer eingeben (aus LIST-Ausgabe)
- Zeigt kompletten Nachrichteninhalt an

Beispiel:
```
Username: bob
Message number: 1
```

### 4. DEL - Nachricht löschen
- Username eingeben
- Nachrichtennummer zum Löschen eingeben

Beispiel:
```
Username: bob
Message number: 1
```

### 5. QUIT - Beenden
- Schließt Verbindung und beendet Client

## Testen

Test-Anleitung-Script für Schritt-für-Schritt-Anweisungen ausführen:
```bash
cat TEST_GUIDE.md
```

## Protokoll-Details

Client und Server kommunizieren über ein Klartext-Protokoll:

- Commands: SEND, LIST, READ, DEL, QUIT
- Parameter getrennt durch Newlines
- Mehrzeilige Nachrichten terminiert durch ".\n"
- Server-Antworten: OK, ERR, oder Daten

## Username-Regeln

- Maximum 8 Zeichen
- Nur Kleinbuchstaben (a-z) und Ziffern (0-9)
- Beispiele: alice, bob123, user42

## Nachrichtenspeicherung

Nachrichten werden im Dateisystem gespeichert:
```
mailspool/
  ├── alice/
  │   ├── 1
  │   └── 2
  └── bob/
      ├── 1
      ├── 2
      └── 3
```

Jeder Benutzer hat sein eigenes Inbox-Verzeichnis, und jede Nachricht ist eine separate nummerierte Datei.

## Fehlerbehandlung

Die Anwendung behandelt:
- Ungültige Usernames
- Netzwerk-Unterbrechungen
- Dateisystem-Fehler
- Ungültige Nachrichtennummern
- Fehlerhafte Commands

## Hinweise

- Der Server ist iterativ (behandelt einen Client zur Zeit)
- Nachrichten bleiben über Server-Neustarts erhalten
- Keine Authentifizierung in dieser Basic-Version
- Betreffzeile limitiert auf 80 Zeichen
- Nachrichteninhalt hat keine Längenbeschränkung

## Problembehandlung

**Port bereits in Verwendung:**
```bash
# Anderen Port wählen oder warten bis alter Prozess Port freigibt
ps aux | grep twmailer-server
kill <pid>
```

**Keine Schreibrechte auf mailspool-Verzeichnis:**
```bash
# Sicherstellen dass Schreibrechte vorhanden sind
chmod 700 mailspool
```

**Verbindung verweigert:**
- Sicherstellen dass Server läuft
- Prüfen dass korrekte IP und Port verwendet werden
- Firewall-Einstellungen prüfen (falls zutreffend)

## Entwicklungsinformation

Für detaillierte Architektur und Entwicklungsinformation siehe:
- `README.md` - Vollständige Dokumentation
- `DESCRIPTION.md` - Projektbeschreibung für Abgabe
