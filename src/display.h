#ifndef DISPLAY_H
#define DISPLAY_H

#include <libgpu.h>

#define OTSIZE          4096
#define PBSIZE          2048
#define SCREEN_RES_X     320
#define SCREEN_RES_Y     240
#define SCREEN_Z         512
#define SCREEN_CENTER_X (SCREEN_RES_X >> 1)
#define SCREEN_CENTER_Y (SCREEN_RES_Y >> 1)

typedef struct {
    DRAWENV draw[2];
    DISPENV disp[2];
    u_long  ot[2][OTSIZE];
    char    primbuff[2][PBSIZE];
    char    *nextprim;
    u_short currbuff;
} DB;

void  screen_init(void);
void  screen_put_buffers(void);
void  screen_swap_buffers(void);

char *get_next_prim(void);
void  advance_next_prim(u_long size);

void  ot_clear(void);
void  ot_add_prim(long otz, void *poly);
void  ot_draw(void);


#endif