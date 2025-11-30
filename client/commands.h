#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

void login(int socket_fd);
void send_message(int socket_fd);
void list_messages(int socket_fd);
void read_message(int socket_fd);
void delete_message(int socket_fd);

#endif // CLIENT_COMMANDS_H