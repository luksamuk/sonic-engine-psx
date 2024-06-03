#include "audio.h"
#include <libspu.h>
#include <libsnd.h>
#include <libcd.h>
#include <stdio.h>

#include "util.h"

// SPU memory management table.
// Allows SPU_MALLOC_MAX calls to SpuMalloc().
static char spu_malloc_rec[SPU_MALLOC_RECSIZ * (SPU_MALLOC_MAX + 1)];

void
audio_init(void)
{
    SpuInit();
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    SpuInitMalloc(SPU_MALLOC_MAX, spu_malloc_rec);

    SpuCommonAttr spusettings;

    // Define XA common attributes
    // Max volume
    spusettings.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX);
    spusettings.mvol.left = 0x3000;
    spusettings.mvol.right = 0x3000;
    spusettings.cd.volume.left = 0x3000;
    spusettings.cd.volume.right = 0x3000;
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

void
set_voice_attr(u_long pitch, u_long channel, u_long addr)
{
    SpuVoiceAttr attr;

    attr.mask = (
        SPU_VOICE_VOLL |
        SPU_VOICE_VOLR |
        SPU_VOICE_PITCH |
        SPU_VOICE_WDSA |
        SPU_VOICE_ADSR_AMODE |
        SPU_VOICE_ADSR_SMODE |
        SPU_VOICE_ADSR_RMODE |
        SPU_VOICE_ADSR_AR |
        SPU_VOICE_ADSR_DR |
        SPU_VOICE_ADSR_SR |
        SPU_VOICE_ADSR_RR |
        SPU_VOICE_ADSR_SL
    );

    attr.voice = channel;                // Low 24 bits are a bit string, 1 bit per voice
    attr.volume.left = 0x0;              // Volume
    attr.volume.right = 0x0;             // Volume
    attr.pitch = pitch;                  // Interval (set pitch)
    attr.addr = addr;                    // Waveform data start address
    attr.a_mode = SPU_VOICE_LINEARIncN;  // Attack rate mode  = Linear increase
    attr.s_mode = SPU_VOICE_LINEARIncN;  // Sustain rate mode = Linear increase
    attr.r_mode = SPU_VOICE_LINEARDecN;  // Release rate mode = Linear decrease
    attr.ar = 0x0;                       // Attack rate
    attr.dr = 0x0;                       // Decay rate
    attr.sr = 0x0;                       // Release rate
    attr.rr = 0x0;                       // Sustain rate
    attr.sl = 0xf;                       // Sustain level

    SpuSetVoiceAttr(&attr);
}

void
send_vag_to_spu(u_long size, u_char *data)
{
    u_long transferred;
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    transferred = SpuWrite(data + sizeof(VAGHeader), size);
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
    printf("Transferred %lu bytes to the SPU.\n", transferred);
}

u_long
spu_vag_transfer(VAGSound *sound)
{
    const VAGHeader *header = (VAGHeader *)sound->file;
    u_int pitch = (SWAP_ENDIAN32(header->samplingFrequency) << 12) / 44100L;
    u_long addr = SpuMalloc(SWAP_ENDIAN32(header->dataSize));
    SpuSetTransferStartAddr(addr);
    send_vag_to_spu(SWAP_ENDIAN32(header->dataSize), sound->file);
    set_voice_attr(pitch, sound->spu_channel, addr);
    return addr;
}

void
audio_vag_load(char *filename, u_long voice, VAGSound *out)
{
    // Load file from CD
    u_long *bytes, length;
    bytes = (u_long *)file_read(filename, &length);
    printf("Read %lu bytes from %s (ptr %p)\n", length, filename, bytes);

    // Create VAGSound
    out->file = (u_char *)bytes;
    out->spu_address = 0;
    out->spu_channel = voice;
    u_long spu_addr = spu_vag_transfer(out);
    out->spu_address = spu_addr;
}

void
audio_vag_play(VAGSound *sound)
{
    // TODO: By then I only need the channel and maybe the
    // sound address on the SPU so I can deallocate it later.
    // Refactor the VAGSound struct.
    SpuVoiceAttr attr;
    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    attr.voice = sound->spu_channel;
    attr.volume.left  = 0x6000;
    attr.volume.right = 0x6000;
    SpuSetVoiceAttr(&attr);
    SpuSetKey(SPU_ON, sound->spu_channel);
}