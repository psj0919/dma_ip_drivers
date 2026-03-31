#ifndef BAR_USER_H
#define BAR_USER_H

#include <stdint.h>

#define BAR_USER_READ   0
#define BAR_USER_WRITE  1

int bar_user_access(int rw, uint64_t address, uint32_t *value);

#endif