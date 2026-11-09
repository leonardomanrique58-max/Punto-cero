/* Plataforma PS2: gsKit (video) + libpad (mando DualShock) */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <libpad.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include "platform.h"
#include "audio.h"

int SW = 640, SH = 448;
static GSGLOBAL *gs;
static char padBuf[256] __attribute__((aligned(64)));
static int padReady = 0;
static unsigned prevDown = 0;

static u64 col(u32c c, int forceOpaque) {
    int a = forceOpaque ? 0x80 : (CA(c) * 128 / 255);
    return GS_SETREG_RGBAQ(CR(c), CG(c), CB(c), a, 0x00);
}

void gfx_init(void) {
    gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT24;
    gs->PSMZ = GS_PSMZ_24;
    gs->ZBuffering = GS_SETTING_ON;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_set_test(gs, GS_ZTEST_ON);
    gsKit_mode_switch(gs, GS_ONESHOT);
    SW = gs->Width;
    SH = gs->Height;
}

void gfx_begin(u32c clear) {
    gs->PrimAlphaEnable = GS_SETTING_OFF;
    gsKit_clear(gs, col(clear, 1));
}

void gfx_end(void) {
    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
}

void gfx_tri(float x1, float y1, int z1, u32c c1, float x2, float y2, int z2, u32c c2, float x3, float y3, int z3, u32c c3) {
    gsKit_prim_triangle_gouraud_3d(gs, x1, y1, z1, x2, y2, z2, x3, y3, z3, col(c1, 1), col(c2, 1), col(c3, 1));
}

void gfx_rect(float x1, float y1, float x2, float y2, u32c c) {
    int translucent = CA(c) < 255;
    gs->PrimAlphaEnable = translucent ? GS_SETTING_ON : GS_SETTING_OFF;
    gsKit_prim_sprite(gs, x1, y1, x2, y2, 0xFFFFFF, col(c, !translucent));
    gs->PrimAlphaEnable = GS_SETTING_OFF;
}

static int waitPad(int port, int slot) {
    int s, tries = 0;
    do { s = padGetState(port, slot); } while (s != PAD_STATE_STABLE && s != PAD_STATE_FINDCTP1 && s != PAD_STATE_DISCONN && ++tries < 200000);
    return s;
}

void pad_init(void) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    if (padPortOpen(0, 0, padBuf) == 0) return;
    waitPad(0, 0);
    padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    waitPad(0, 0);
    padReady = 1;
}

static float axis(unsigned char v) {
    float f = ((int)v - 128) / 127.0f;
    if (f > -0.18f && f < 0.18f) return 0;
    return f > 0 ? (f - 0.18f) / 0.82f : (f + 0.18f) / 0.82f;
}

void pad_poll(Pad *p) {
    struct padButtonStatus b;
    memset(p, 0, sizeof(*p));
    if (!padReady) return;
    int st = padGetState(0, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) return;
    if (padRead(0, 0, &b) == 0) return;
    unsigned m = 0xFFFF ^ b.btns; /* los botones llegan en 0 al presionar */
    unsigned d = 0;
    if (m & PAD_UP) d |= B_UP;       if (m & PAD_DOWN) d |= B_DOWN;
    if (m & PAD_LEFT) d |= B_LEFT;   if (m & PAD_RIGHT) d |= B_RIGHT;
    if (m & PAD_CROSS) d |= B_CROSS; if (m & PAD_CIRCLE) d |= B_CIRCLE;
    if (m & PAD_SQUARE) d |= B_SQUARE; if (m & PAD_TRIANGLE) d |= B_TRIANGLE;
    if (m & PAD_L1) d |= B_L1;       if (m & PAD_R1) d |= B_R1;
    if (m & PAD_L2) d |= B_L2;       if (m & PAD_R2) d |= B_R2;
    if (m & PAD_START) d |= B_START; if (m & PAD_SELECT) d |= B_SELECT;
    p->down = d; p->pressed = d & ~prevDown; prevDown = d;
    if (b.mode >> 4 == 0x7) { /* modo analogico */
        p->lx = axis(b.ljoy_h); p->ly = axis(b.ljoy_v);
        p->rx = axis(b.rjoy_h); p->ry = axis(b.rjoy_v);
    }
}

int main(void) {
    int fps;
    float dt;
    SifInitRpc(0);
    gfx_init();
    pad_init();
    fps = (gs->Mode == GS_MODE_PAL) ? 50 : 60;      /* PAL = 50 Hz, NTSC = 60 Hz */
    dt = 1.0f / fps;
    audio_init(fps);
    game_init();
    for (;;) {
        Pad p;
        pad_poll(&p);
        game_frame(&p, dt);
        audio_frame(game_music_mul());
    }
    return 0;
}
