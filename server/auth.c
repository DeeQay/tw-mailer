#include "auth.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <ldap.h>

// Authentifiziert Benutzer gegen LDAP-Server
// Rückgabe: 1 bei Erfolg, 0 bei Auth-Fehler, -1 bei Server nicht erreichbar
int ldap_authenticate(const char *username, const char *password) {
    LDAP *ld = NULL;
    int result = 0;
    int ldap_version3 = LDAP_VERSION3;
    
    // Erstelle LDAP URI
    char ldap_uri[256];
    snprintf(ldap_uri, sizeof(ldap_uri), "ldap://%s:%d", LDAP_HOST, LDAP_PORT);
    
    // Initialisiere LDAP-Verbindung
    int rc = ldap_initialize(&ld, ldap_uri);
    if (rc != LDAP_SUCCESS) {
        fprintf(stderr, "ldap_initialize failed: %s\n", ldap_err2string(rc));
        return -1;
    }
    
    // LDAP Version 3
    rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &ldap_version3);
    if (rc != LDAP_OPT_SUCCESS) {
        fprintf(stderr, "ldap_set_option failed: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return -1;
    }
    
    // Netzwerk-Timeout (5 Sekunden)
    struct timeval network_timeout;
    network_timeout.tv_sec = 5;
    network_timeout.tv_usec = 0;
    rc = ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &network_timeout);
    if (rc != LDAP_OPT_SUCCESS) {
        fprintf(stderr, "ldap_set_option (timeout) failed: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ld, NULL, NULL);
        return -1;
    }
    
    // Erstelle DN für Benutzer: uid=<username>,ou=people,dc=technikum-wien,dc=at
    char bind_dn[MAX_PATH];
    snprintf(bind_dn, sizeof(bind_dn), "uid=%s,ou=people,%s", username, LDAP_SEARCH_BASE);
    
    // Erstelle Credentials
    struct berval cred; 
    cred.bv_val = (char *)password; 
    cred.bv_len = strlen(password);
    
    // Versuche LDAP bind (Authentifizierung)
    rc = ldap_sasl_bind_s(ld, bind_dn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL); 
    
    if (rc == LDAP_SUCCESS) {
        result = 1;
        printf("LDAP authentication successful for user: %s\n", username);
    } else if (rc == LDAP_SERVER_DOWN || rc == LDAP_TIMEOUT || rc == LDAP_CONNECT_ERROR) {
        fprintf(stderr, "LDAP server unreachable: %s\n", ldap_err2string(rc));
        result = -1;
    } else {
        fprintf(stderr, "LDAP authentication failed for user %s: %s\n", 
                username, ldap_err2string(rc));
        result = 0;
    }
    
    // Schließe LDAP-Verbindung
    ldap_unbind_ext_s(ld, NULL, NULL);
    return result;
}