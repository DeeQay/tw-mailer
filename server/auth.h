#ifndef SERVER_AUTH_H
#define SERVER_AUTH_H

int ldap_authenticate(const char *username, const char *password);

#endif // SERVER_AUTH_H