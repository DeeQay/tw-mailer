# TW-Mailer: Finale Projekt-Zusammenfassung

## 1. Finale Checkliste

| Anforderung | Status | Implementierung |
| :--- | :---: | :--- |
| Server concurrent mit `fork()` | ✅ | `fork()` in `main()`, `SIGCHLD` Handler für Zombies |
| Synchronisation kritischer Sektionen | ✅ | `flock()` auf User-Inbox und Blacklist |
| LOGIN mit LDAP | ✅ | `ldap_authenticate()` gegen `ldap.technikum.wien.at:389` |
| Nur auth. User nutzen Commands | ✅ | `session.authenticated` Check in `handle_client()` |
| 3 Login-Versuche erlaubt | ✅ | `MAX_LOGIN_ATTEMPTS=3`, `register_failed_login()` |
| IP-Blacklist für 1 Minute | ✅ | `BLACKLIST_DURATION=60`, `is_ip_blacklisted()` |
| Blacklist persistent | ✅ | `blacklist.dat` Datei im mail-spool-dir |
| SEND: Sender aus Session | ✅ | `handle_send()` nutzt `session->username` |
| LIST: Username aus Session | ✅ | `handle_list()` nutzt Session (kein Param) |
| READ: Username aus Session | ✅ | `handle_read()` nutzt Session (nur MsgNum) |
| DEL: Username aus Session | ✅ | `handle_del()` nutzt Session (nur MsgNum) |
| Protokoll-Spezifikation | ✅ | LOGIN, SEND, LIST, READ, DEL, QUIT |
| Makefile (all/clean) | ✅ | `-lldap` `-llber` Flags inkludiert |
| Kommentierter Code | ✅ | Ausführliche Kommentare in Deutsch |

---

## 2. Validierung & Design-Entscheidungen

### QUIT-Command Validierung
* **Prüfung:** `QUIT` muss auch ohne vorherigen Login funktionieren.
* **Ergebnis:** ✅ Code geprüft (Lines 720-780). `QUIT` wird vor dem Auth-Check verarbeitet.

### Blacklist Logik (User vs. IP)
* **Aufgabe:** "only 3 login attempts are allowed per user and IP".
* **Entscheidung:** Implementierung von **3 Versuchen pro IP** (unabhängig vom User).
* **Begründung:** Diese Variante ist sicherer, da sie Brute-Force-Angriffe effektiver unterbindet als eine Kombination aus User+IP.

---

## 3. Zusammenfassung der Implementierung

### Server (`twmailer-server.c`)
* **Concurrency:** Implementiert mittels `fork()` und Zombie-Prozess-Handling (`SIGCHLD`).
* **LDAP-Authentifizierung:** Nutzung der Funktion `ldap_authenticate()` gegen Port 389.
* **Session-Management:** Interne Struktur speichert `authenticated` Flag und `username`.
* **Blacklist-System:**
    * Persistente Speicherung in `blacklist.dat`.
    * Synchronisation mittels `flock()`.
    * Logik: 3 Fehlversuche $\rightarrow$ 1 Minute IP-Sperre.
* **Protokoll-Anpassungen:**
    * **LOGIN:** Neu (LDAP).
    * **SEND/LIST/READ/DEL:** Parameter reduziert, Nutzeridentifikation läuft ausschließlich über die Session.
    * **QUIT:** Immer erlaubt.

### Client (`twmailer-client.c`)
* **Login:** Sichere Passworteingabe ohne Echo.
* **Status:** Tracking via globaler Variable `is_logged_in`.
* **Menü:** 6 Optionen (LOGIN, SEND, LIST, READ, DEL, QUIT).
* **Commands:** Anpassung der Parameterübergabe (kein manueller User-Input mehr für Sender/User).

### Synchronisation & Build
* **User-Inbox:** `flock()` Schutz vor Schreiben (SEND) und Löschen (DEL).
* **Blacklist:** `flock()` Schutz beim Lesen/Schreiben der Sperrdatei.
* **Makefile:** Linker-Flags `-lldap -llber` hinzugefügt (benötigt `libldap2-dev`).