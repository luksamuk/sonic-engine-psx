#include "render.h"

#include <libgpu.h>
#include <stdio.h>
#include <malloc.h>

#include "util.h"

void
load_texture(char *filename, u_long *mode, u_short *tpage, u_short *clut)
{
    u_long *bytes;
    u_long length;
    TIM_IMAGE tim;

    bytes = (u_long *)file_read(filename, &length);
    printf("Read %lu bytes from %s (ptr %p)\n", length, filename, bytes);

    OpenTIM(bytes);
    ReadTIM(&tim);

    LoadImage(tim.prect, tim.paddr);
    DrawSync(0);

    if(tim.mode & 0x8) {
        LoadImage(tim.crect, tim.caddr);
        DrawSync(0);
    }

    *mode = tim.mode;
    *tpage = getTPage(tim.mode & 0x3, 0, tim.prect->x, tim.prect->y);
    *clut = getClut(tim.crect->x, tim.crect->y);

    free3(bytes);
}

