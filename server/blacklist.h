#ifndef SERVER_BLACKLIST_H
#define SERVER_BLACKLIST_H

#include "common.h"

int acquire_blacklist_lock(void);
void release_blacklist_lock(int lock_fd);
int load_blacklist_entry(const char *ip, BlacklistEntry *entry);
void save_blacklist_entry(const char *ip, int attempts, time_t block_until);
int get_blacklist_remaining(const char *ip);
int register_failed_login(const char *ip);
void register_successful_login(const char *ip);

#endif // SERVER_BLACKLIST_H