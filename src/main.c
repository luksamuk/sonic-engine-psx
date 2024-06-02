#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <stdlib.h>
#include <libcd.h>
#include <stdio.h>
#include <libsnd.h>
#include <libspu.h>
#include <libmcx.h>

#include "display.h"
#include "render.h"
#include "util.h"
#include "camera.h"
#include "audio.h"

extern char __heap_start, __sp;


#define SPR_FRAME_SIZE    60
#define SPR_RADIUS      (SPR_FRAME_SIZE >> 1)

#define X_ACCEL            192
#define X_FRICTION         192
#define X_DECEL           2048
#define X_TOP_SPD        24576
#define Y_GRAVITY          896
#define Y_MIN_JUMP       16384
#define Y_JUMP_STRENGTH  26624



static SVECTOR v[] = {
    {-SPR_RADIUS, -SPR_RADIUS, 0},
    { SPR_RADIUS, -SPR_RADIUS, 0},
    {-SPR_RADIUS,  SPR_RADIUS, 0},
    { SPR_RADIUS,  SPR_RADIUS, 0},
};

static short elements[] = {
    0, 1, 2, 3,
};

static short walking_anim[] = {
    1, 1,    1, 2,    1, 3,
    2, 1,    2, 0,    3, 0,
};

static short jumping_anim[] = {
    3, 3,    4, 0,
    4, 1,    4, 0,
    4, 2,    4, 0,
    4, 3,    4, 0,
};

VECTOR  position = {0, 0, 491520};
SVECTOR rot      = {0, 0, 0};
VECTOR  spd      = {0, 0, 0};
VECTOR  acc      = {0, 0, 0};

POLY_FT4 *poly;
VECTOR  pos      = {0, 300, 0};
VECTOR  scale    = {ONE, ONE, ONE};
Camera  camera;
VECTOR  camera2d = {0, 300, 0};
MATRIX  world    = {0};
MATRIX  view     = {0};
u_long  padstate;

u_long  sonic0_mode;
u_short sonic0_tpage;
u_short sonic0_clut;

u_long  sonic1_mode;
u_short sonic1_tpage;
u_short sonic1_clut;

u_short sonic_frame_x;
u_short sonic_frame_y;

u_short direction = 0;
u_short framenum  = 0;
u_short framedur  = 0;
u_short ground    = 0;
u_short roll      = 0;
u_short stopstate = 0;

// ------------------

static int looked = 0;

static void
draw_scene(void)
{
    long p, otz, flg;
    int nclip;

    if(!looked) {
        look_at(&camera, &camera.position, &camera2d, &(VECTOR){0, -ONE, 0});
        looked = 1;
    }

    rot.vx = -(ONE >> 2);
    pos.vx = position.vx >> 12;
    //pos.vy = position.vx >> 12;
    pos.vz = position.vz >> 12;

    RotMatrix(&rot, &world);
    TransMatrix(&world, &pos);
    ScaleMatrix(&world, &scale);

    CompMatrixLV(&camera.lookat, &world, &view);

    SetRotMatrix(&view);
    SetTransMatrix(&view);

    poly = (POLY_FT4*)get_next_prim();
    setPolyFT4(poly);
    setRGB0(poly, 128, 128, 128);
    poly->tpage = sonic0_tpage;
    poly->clut  = sonic0_clut;
 
    if(roll) {
        sonic_frame_x = jumping_anim[framenum << 1];
        sonic_frame_y = jumping_anim[(framenum << 1) + 1];
    } else if(abs(spd.vx) > 0){
        sonic_frame_x = walking_anim[framenum << 1];
        sonic_frame_y = walking_anim[(framenum << 1) + 1];
    } else {
        switch(stopstate) {
        case 1: // look up
            sonic_frame_x = 6;
            sonic_frame_y = 1;
            break;
        case 2: // crouch down
            sonic_frame_x = 5;
            sonic_frame_y = 3;
            break;
        default:
            sonic_frame_x = sonic_frame_y = 0;
            break;
        }
    }

    if(sonic_frame_x > 3) {
        poly->tpage = sonic1_tpage;
        poly->clut  = sonic1_clut;
        sonic_frame_x -= 4;
    }

    u_short x_offset = (sonic_frame_x * SPR_FRAME_SIZE);
    u_short y_offset = (sonic_frame_y * SPR_FRAME_SIZE);

    if(!direction) {
        setUV4(poly,
            x_offset,      y_offset,
            x_offset + 59, y_offset,
            x_offset,      y_offset + 59,
            x_offset + 59, y_offset + 59);
    } else {
        setUV4(poly,
            x_offset + 59, y_offset,
            x_offset,      y_offset,
            x_offset + 59, y_offset + 59,
            x_offset,      y_offset + 59);
    }

    nclip = RotAverageNclip4(
        &v[elements[0]],
        &v[elements[1]],
        &v[elements[2]],
        &v[elements[3]],
        (long*) &poly->x0,
        (long*) &poly->x1,
        (long*) &poly->x2,
        (long*) &poly->x3,
        &p, &otz, &flg);
    
    if(nclip <= 0) return;

    if ((otz > 0) && (otz < OTSIZE)) {
        ot_add_prim(otz, poly);
        advance_next_prim(sizeof(POLY_FT4));
    }
}

static void
screen_draw()
{
    screen_put_buffers();
    draw_scene();

    ot_draw();
    screen_swap_buffers();
}

static void
screen_update()
{
    if(padstate & _PAD(0, PADLright)) {
        if(spd.vx < 0) {
            acc.vx = X_DECEL;
        } else {
            acc.vx = X_ACCEL;
            direction = 0;
        }
    }
    else if(padstate & _PAD(0, PADLleft)) {
        if(spd.vx > 0) {
            acc.vx = -X_DECEL;
        } else {
            acc.vx = -X_ACCEL;
            direction = 1;
        }
    } else {
        if(spd.vx > 0) {
            acc.vx = -X_FRICTION;
        } else if(spd.vx < 0) { 
            acc.vx = X_FRICTION;
        } else {
            acc.vx = 0;

            if(padstate & _PAD(0, PADLup)) {
                stopstate = 1;
            } else if(padstate & _PAD(0, PADLdown)) {
                stopstate = 2;
            } else stopstate = 0;
        }

        if(abs(spd.vx) < X_FRICTION) {
            spd.vx = 0;
        }
    }

    if(spd.vx > X_TOP_SPD) spd.vx = X_TOP_SPD;
    else if(spd.vx < -X_TOP_SPD) spd.vx = -X_TOP_SPD;


    if(position.vz > -122880) {
        acc.vy = -Y_GRAVITY;
    } else {
        position.vz = -122880;
        acc.vy = 0;
        ground = 1;
        roll = 0;
    }

    if(ground && (padstate & _PAD(0, PADRdown))) {
        ground = 0;
        roll = 1;
        spd.vy = Y_JUMP_STRENGTH;
        framenum = 0; // reset anim
        SpuSetKey(SPU_ON, SPU_0CH);
        printf("Jump\n");
    }

    if(!ground
       && roll
       && !(padstate & _PAD(0, PADRdown))
       && (spd.vy > Y_MIN_JUMP))
    {
        spd.vy = Y_MIN_JUMP;
    }

    spd.vx += acc.vx;
    spd.vy += acc.vy;

    position.vx += spd.vx;
    position.vz += spd.vy;

    u_short maxframe = roll ? 8 : 6;
    if(framedur == 0) {
        framenum++;
        framedur = 5;
        if(framenum >= maxframe) {
            framenum = 0;
        }
    } else framedur--;

    // Camera controls
    if(padstate & _PAD(0, PADRup)) {
        camera.position.vy -= 10;
        look_at(&camera, &camera.position, &camera2d, &(VECTOR){0, -ONE, 0});
    }

    if(padstate & _PAD(0, PADRright)) {
        camera.position.vy += 10;
        if(camera.position.vy > 0) camera.position.vy = 0;
        look_at(&camera, &camera.position, &camera2d, &(VECTOR){0, -ONE, 0});
    }
}

void
load_sfx(unsigned long voice, char *filename)
{
    SpuVoiceAttr  spuvoice;
    u_long *bytes;
    u_long length;
    u_long vag_addr;

    // Load .VAG file
    bytes = (u_long *)file_read(filename, &length);
    printf("Read %lu bytes from %s (ptr %p)\n", length, filename, bytes);
    
    vag_addr = SpuMalloc(length);

    if(SpuSetTransferStartAddr(vag_addr) == 0) {
        printf("Error setting transfer start address for %s!\n", filename);
    }
    printf("SPU .VAG address: 0x%x\n", vag_addr);
    printf("SPU given .VAG start address: 0x%x\n", SpuGetTransferStartAddr());

    u_long transferred = SpuWrite((u_char*)bytes, length);
    printf("Transfering %ld bytes to the SPU. Transfered so far: %ld bytes\n", length, transferred);
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT); // Await completion
    printf("SPU transfer completed.\n");
    free(bytes);

    // Define voice common attributes
    spuvoice.mask = (
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

    // using channel 2 so it doesn't conflict with XA playback
    spuvoice.voice = voice;
    spuvoice.volume.left = 0x1fff;
    spuvoice.volume.right = 0x1fff;
    spuvoice.pitch = 0x1000;
    spuvoice.addr = vag_addr;
    spuvoice.a_mode = SPU_VOICE_LINEARIncN;
    spuvoice.s_mode = SPU_VOICE_LINEARIncN;
    spuvoice.r_mode = SPU_VOICE_LINEARDecN;
    spuvoice.ar = 0x0;
    spuvoice.dr = 0x0;
    spuvoice.sr = 0x0;
    spuvoice.rr = 0x0;
    spuvoice.sl = 0xf;

    SpuSetVoiceAttr(&spuvoice);
    SpuSetKey(SPU_ON, voice);
}

int main(void) {
    InitHeap3((unsigned long *) (&__heap_start), (&__sp - 0x5000) - &__heap_start);

    screen_init();

    // Load font
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(16, 16, 320, 64, 0, 512));

    // Camera
    camera.position.vx = 0;
    camera.position.vy = 0; // Y grows down
    camera.position.vz = -100; // Push the camera back further
    camera.lookat = (MATRIX){0};

    CdInit();
    PadInit(0);
    audio_init();
    srand(0);

    load_texture("\\TEXTURES\\SONIC-0.TIM;1", &sonic0_mode, &sonic0_tpage, &sonic0_clut);
    load_texture("\\TEXTURES\\SONIC-1.TIM;1", &sonic1_mode, &sonic1_tpage, &sonic1_clut);
    load_sfx(SPU_0CH, "\\SFX\\JUMP.VAG;1");
    audio_xa_stream("\\BGM\\BGM001.XA;1", 0, 0);

    while (1) {
        padstate = 0;
        padstate = PadRead(0);

        ot_clear();

        FntPrint("X     %08x\nZ     %08x\nD     %08x\n\n", position.vx, position.vz, position.vy);
        FntPrint("SPD.X %08x\nSPD.Y %08x\n", spd.vx, spd.vy);
        FntPrint("ACC.X %08x\nACC.Y %08x\n", acc.vx, acc.vy);

        screen_update();
        screen_draw();
        FntFlush(-1);
    }

    return 0;
}
