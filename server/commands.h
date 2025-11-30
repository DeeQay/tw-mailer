#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H

#include "common.h"

void handle_send(int client_socket, const Session *session);
void handle_list(int client_socket, const Session *session);
void handle_read(int client_socket, const Session *session);
void handle_del(int client_socket, const Session *session);

#endif // SERVER_COMMANDS_H