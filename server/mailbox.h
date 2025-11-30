#ifndef SERVER_MAILBOX_H
#define SERVER_MAILBOX_H

#include "common.h"

int acquire_user_lock(const char *username);
void release_user_lock(int lock_fd);
int validate_username(const char *username);
int create_user_inbox(const char *username);
int get_next_message_number(const char *username);

#endif // SERVER_MAILBOX_H