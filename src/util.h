#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

#define SWAP_ENDIAN32(x) (((x)>>24) | (((x)>>8) & 0xFF00) | (((x)<<8) & 0x00FF0000) | ((x)<<24))

char *file_read(char *filename, u_long *length);

#endif