#include "display.h"
#include <libgpu.h>
#include <libetc.h>

static DB screen;

void
screen_init(void)
{
    // Reset GPU
    ResetGraph(0);

    // Set display and draw areas of buffers
    SetDefDispEnv(&screen.disp[0], 0, 0, SCREEN_RES_X, SCREEN_RES_Y);
    SetDefDrawEnv(&screen.draw[0], 0, 240, SCREEN_RES_X, SCREEN_RES_Y);
    SetDefDispEnv(&screen.disp[1], 0, 240, SCREEN_RES_X, SCREEN_RES_Y);
    SetDefDrawEnv(&screen.draw[1], 0, 0, SCREEN_RES_X, SCREEN_RES_Y);

    // Set draw buffers as background buffers
    screen.draw[0].isbg = 1;
    screen.draw[1].isbg = 1;

    // Set background clear color
    setRGB0(&screen.draw[0], 63, 0, 127);
    setRGB0(&screen.draw[1], 63, 0, 127);

    // Set current initial buffer
    screen.currbuff = 0;
    PutDispEnv(&screen.disp[screen.currbuff]);
    PutDrawEnv(&screen.draw[screen.currbuff]);

    // Initialize and setup the GTE geometry offsets
    InitGeom();
    SetGeomOffset(SCREEN_CENTER_X, SCREEN_CENTER_Y);
    SetGeomScreen(SCREEN_Z);

    // Enable display
    SetDispMask(1);

    // Reset primitive buffer
    screen.nextprim = screen.primbuff[screen.currbuff];
}

void
screen_put_buffers(void)
{
    DrawSync(0);
    VSync(0);
    PutDispEnv(&screen.disp[screen.currbuff]);
    PutDrawEnv(&screen.draw[screen.currbuff]);
}

void
screen_swap_buffers(void)
{
    screen.currbuff = !screen.currbuff;
    screen.nextprim = screen.primbuff[screen.currbuff];
}

char *
get_next_prim(void)
{
    return screen.nextprim;
}

void
advance_next_prim(u_long size)
{
    screen.nextprim += size;
}

void
ot_clear(void)
{
    ClearOTagR(screen.ot[screen.currbuff], OTSIZE);
}

void
ot_add_prim(long otz, void *poly)
{
    AddPrim(&screen.ot[screen.currbuff][otz], poly);
}

void
ot_draw(void)
{
    DrawOTag(&screen.ot[screen.currbuff][OTSIZE - 1]);
}