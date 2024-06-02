#ifndef AUDIO_H
#define AUDIO_H

#include <sys/types.h>

#define SPU_MALLOC_MAX 10

void audio_init(void);
void audio_xa_stream(char *filename, int doublespeed, u_char channel);

#endif