#include "audio.h"
#include <libspu.h>
#include <libsnd.h>
#include <libcd.h>
#include <stdio.h>

static char spu_malloc_managetable[SPU_MALLOC_RECSIZ * (SPU_MALLOC_MAX + 1)];

void
audio_init(void)
{
    SpuInit();
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    SpuInitMalloc(SPU_MALLOC_MAX, spu_malloc_managetable); // Setup number of mallocs and management table in RAM

    SpuCommonAttr spusettings;

    // Define XA common attributes
    // Max volume
    spusettings.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX);
    spusettings.mvol.left = 0x6000;
    spusettings.mvol.right = 0x6000;
    spusettings.cd.volume.left = 0x6000;
    spusettings.cd.volume.right = 0x6000;
    spusettings.cd.mix = SPU_ON; // Enable CD input for playback

    SpuSetCommonAttr(&spusettings);
    SpuSetIRQ(SPU_OFF);
    SpuSetKey(SPU_OFF, SPU_ALLCH);
}

void
audio_xa_stream(char *filename, int doublespeed, u_char channel)
{
    CdlFILE filepos;
    int track_start, track_end;

    if(CdSearchFile(&filepos, filename) == NULL) {
        printf("XA file %s not found!", filename);
        return;
    }

    track_start = CdPosToInt(&filepos.pos);
    track_end   = track_start + (filepos.size / 2048) - 1;

    // Set-up drive speed, ADPCM play, Subheader filter, sector size.
    // If using double speed, would have to load a XA file with 8 channels
    // In single speed, a 4 channels XA is used
    u_char param[4];

    param[0] = CdlModeRT | CdlModeSF | CdlModeSize1; // 1x speed
    if(doublespeed) {
        param[0] |= CdlModeSpeed;
    }

    // Issue primitive command to CD-ROM system (blocking)
    CdControlB(CdlSetmode, param, 0);
    CdControlF(CdlPause, 0); // Pause

    // Specify filter (file and channel number to actually read data from)
    CdlFILTER filter;
    filter.file = 1; // Always 1
    filter.chan = channel; // 0-3 on 1x, 0-7 on 2x
    CdControlF(CdlSetfilter, (u_char*) &filter);

    // Begin playback
    CdlLOC loc;
    CdIntToPos(track_start, &loc);
    CdControlF(CdlReadS, (u_char *)&loc);
}