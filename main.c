#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <stdlib.h>
#include <libcd.h>
#include <libgpu.h>
#include <stdio.h>
#include <libspu.h>

extern char __heap_start, __sp;

#define OTSIZE          4096
#define PBSIZE          2048
#define SCREEN_RES_X     320
#define SCREEN_RES_Y     240
#define SCREEN_Z         512
#define SPR_FRAME_SIZE    60
#define SPR_RADIUS        (SPR_FRAME_SIZE >> 1)
#define SCREEN_CENTER_X (SCREEN_RES_X >> 1)
#define SCREEN_CENTER_Y (SCREEN_RES_Y >> 1)

#define X_ACCEL            192
#define X_FRICTION         192
#define X_DECEL           2048
#define X_TOP_SPD        24576
#define Y_GRAVITY          896
#define Y_MIN_JUMP       16384
#define Y_JUMP_STRENGTH  26624

typedef struct {
    DRAWENV draw[2];
    DISPENV disp[2];
    u_long  ot[2][OTSIZE];
    char    primbuff[2][PBSIZE];
    char    *nextprim;
    u_short currbuff;
} DB;

static DB screen;

static SVECTOR v[] = {
    {-SPR_RADIUS, -SPR_RADIUS, 0},
    { SPR_RADIUS, -SPR_RADIUS, 0},
    {-SPR_RADIUS,  SPR_RADIUS, 0},
    { SPR_RADIUS,  SPR_RADIUS, 0},
};

static short elements[] = {
    0, 1, 2, 3,
};

/*static short elements[] = {
    1, 3, 0, 2,
};*/

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

typedef struct {
    VECTOR  position;
    SVECTOR rotation;
    MATRIX  lookat;
} Camera;

VECTOR  position = {0, 0, 491520};
SVECTOR rot      = {0, 0, 0};
VECTOR  spd      = {0, 0, 0};
VECTOR  acc      = {0, 0, 0};

POLY_FT4 *poly;
VECTOR  pos      = {0, 300, 0};
VECTOR  scale    = {ONE, ONE, ONE};
Camera  camera;
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

// --- audio bits ---
// 4 = simple speed, 8 = double speed
#define XA_SECTOR_OFFSET 4

typedef struct {
    int start;
    int end;
} XATrack;

SpuCommonAttr spuSettings;
CdlFILE xapos = {0};

// ------------------

char *
file_read(char *filename, u_long *length)
{
    CdlFILE filepos;
    int numsectors;
    char *buffer;

    buffer = NULL;

    if(CdSearchFile(&filepos, filename) == NULL) {
        printf("File %s not found!", filename);
        return NULL;
    }

    numsectors = (filepos.size + 2047) / 2048;
    buffer = (char*) malloc3(2048 * numsectors);
    if(!buffer) {
        printf("Error allocating %d sectors.\n", numsectors);
        return NULL;
    }

    CdControl(CdlSetloc, (u_char*) &filepos.pos, 0);
    CdRead(numsectors, (u_long*) buffer, CdlModeSpeed);
    CdReadSync(0, 0);

    *length = filepos.size;
    return buffer;
}

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

    free(bytes);
}

void
look_at(Camera *camera, VECTOR *eye, VECTOR *target, VECTOR *up)
{
    VECTOR xright;
    VECTOR yup;
    VECTOR zforward;

    VECTOR x, y, z; // Normalized right/up/forward vectors

    VECTOR pos; // Temporary position
    VECTOR t;   // Temporary translation

    zforward.vx = target->vx - eye->vx;
    zforward.vy = target->vy - eye->vy;
    zforward.vz = target->vz - eye->vz;
    VectorNormal(&zforward, &z);

    OuterProduct12(&z, up, &xright);
    VectorNormal(&xright, &x);

    OuterProduct12(&z, &x, &yup);
    VectorNormal(&yup, &y);

    camera->lookat.m[0][0] = x.vx;
    camera->lookat.m[0][1] = x.vy;
    camera->lookat.m[0][2] = x.vz;

    camera->lookat.m[1][0] = y.vx;
    camera->lookat.m[1][1] = y.vy;
    camera->lookat.m[1][2] = y.vz;

    camera->lookat.m[2][0] = z.vx;
    camera->lookat.m[2][1] = z.vy;
    camera->lookat.m[2][2] = z.vz;

    pos.vx = -eye->vx;
    pos.vy = -eye->vy;
    pos.vz = -eye->vz;

    ApplyMatrixLV(&camera->lookat, &pos, &t);
    TransMatrix(&camera->lookat, &t);
}

static void
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

    // Load font
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(16, 16, 320, 64, 0, 512));

    // Camera
    camera.position.vx = 0;
    camera.position.vy = 0; // Y grows down
    camera.position.vz = -100; // Push the camera back further
    camera.lookat = (MATRIX){0};
}

static int looked = 0;

static void
draw_scene(void)
{
    long p, otz, flg;
    int nclip;

    if(!looked) {
        look_at(&camera, &camera.position, &pos, &(VECTOR){0, -ONE, 0});
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

    poly = (POLY_FT4*)screen.nextprim;
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
        AddPrim(&screen.ot[screen.currbuff][otz], poly);
        screen.nextprim += sizeof(POLY_FT4);
    }
}

static void
screen_draw()
{
    DrawSync(0);
    VSync(0);

    PutDispEnv(&screen.disp[screen.currbuff]);
    PutDrawEnv(&screen.draw[screen.currbuff]);

    draw_scene();

    DrawOTag(&screen.ot[screen.currbuff][OTSIZE - 1]);
    screen.currbuff = !screen.currbuff;
    screen.nextprim = screen.primbuff[screen.currbuff];
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
}

void
sound_init(void)
{
    SpuInit();

    SpuCommonAttr spusettings;

    // Max volume
    spusettings.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX);
    spusettings.mvol.left = 0x6000;
    spusettings.mvol.right = 0x6000;
    spusettings.cd.volume.left = 0x6000;
    spusettings.cd.volume.right = 0x6000;

    // enable cd input
    spusettings.cd.mix = SPU_ON;
    SpuSetCommonAttr(&spusettings);
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
}

void
load_music(void)
{
    CdlFILE filepos;
    XATrack track;
    char *filename = "\\AUDIO\\BGM001.XA;1";

    if(CdSearchFile(&filepos, filename) == NULL) {
        printf("File %s not found!", filename);
        return;
    }

    track.start = CdPosToInt(&filepos.pos);
    track.end   = track.start + (filepos.size / 2048) - 1;

    // XA Setup.
    // Drive speed, ADPCM play, Subheader filter, sector size
    // If using double speed, would have to load a XA file with 8 channels
    // In single speed, a 4 channels XA is used
    u_char param[4];
    param[0] = CdlModeRT | CdlModeSF | CdlModeSize1; // 1x speed
    //param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF | CdlModeSize1; // 2x speed
    // Issue primitive command to CD-ROM system (blocking)
    CdControlB(CdlSetmode, param, 0);
    CdControlF(CdlPause, 0); // Pause
    printf("Param: %02x %02x %02x %02x\n", param[0], param[1], param[2], param[3]);

    // Specify filter (file and channel number to actually read data from)
    CdlFILTER filter;
    filter.file = 1;
    filter.chan = 0;
    CdControlF(CdlSetfilter, (u_char*) &filter);

    // File position on CD
    CdlLOC loc;
    // Begin playback
    CdIntToPos(track.start, &loc);
    CdControlF(CdlReadS, (u_char *)&loc);
}

int main(void) {
    InitHeap3((unsigned long *) (&__heap_start), (&__sp - 0x5000) - &__heap_start);

    screen_init();
    CdInit();
    PadInit(0);
    sound_init();
    srand(0);

    load_texture("\\TEXTURES\\SONIC-0.TIM;1", &sonic0_mode, &sonic0_tpage, &sonic0_clut);
    load_texture("\\TEXTURES\\SONIC-1.TIM;1", &sonic1_mode, &sonic1_tpage, &sonic1_clut);
    load_music();

    while (1) {
        padstate = 0;
        padstate = PadRead(0);

        ClearOTagR(screen.ot[screen.currbuff], OTSIZE);

        FntPrint("X %08x\nZ %08x\nD %08x\n\n", position.vx, position.vz, position.vy);
        FntPrint("SPD.X %08x\nSPD.Y %08x\n", spd.vx, spd.vy);
        FntPrint("ACC.X %08x\nACC.Y %08x\n", acc.vx, acc.vy);

        screen_update();
        screen_draw();
        FntFlush(-1);
    }

    return 0;
}
