#ifndef AUDIO_H
#define AUDIO_H

#include <sys/types.h>

#define SPU_MALLOC_MAX 8

typedef struct VAGSound {
    u_char *file;
    u_long spu_channel;
    u_long spu_address;
} VAGSound;

typedef struct VAGHeader {
    char id[4];
    unsigned int version;
    unsigned int reserved;
    unsigned int dataSize;
    unsigned int samplingFrequency;
    char reserved2[12];
    char name[16];
} VAGHeader;

void audio_init(void);
void audio_xa_stream(char *filename, int doublespeed, u_char channel);
void audio_vag_load(char *filename, u_long voice, VAGSound *out);
void audio_vag_play(VAGSound *sound);

#endif