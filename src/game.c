/* Punto Cero (PS2): shooter tactico por rondas contra bots.
 * Render 3D por software (transformacion + recorte) y dibujo de triangulos con gsKit. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "audio.h"

#define PI 3.14159265f
#define HALF 30.0f
#define MAXBOX 96
#define MAXBOT 10
#define SKY RGBA(201, 183, 156, 255)

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float rndf(float a, float b) { return a + (rand() / (float)RAND_MAX) * (b - a); }

/* ---------- Configuracion ---------- */
typedef struct { int diff, bots, rounds, sens, fov, invertY, cross, musicV, sfxV; } Settings;
static Settings S = { 1, 4, 5, 5, 80, 0, 0, 3, 4 };
static const char *DIFF_NAMES[] = { "FACIL", "NORMAL", "DIFICIL" };
static const float DIFF_HP[] = { 70, 100, 130 }, DIFF_REACT[] = { 0.85f, 0.5f, 0.28f };
static const float DIFF_AIM[] = { 0.16f, 0.09f, 0.045f }, DIFF_DMG[] = { 6, 9, 12 };
static const float DIFF_FIRE[] = { 0.55f, 0.38f, 0.26f }, DIFF_SPEED[] = { 3.0f, 3.6f, 4.2f };
static const int ROUND_OPTS[] = { 3, 5, 8, 12 };
static const char *CROSS_NAMES[] = { "BLANCO", "VERDE", "CIAN", "NARANJA", "ROJO" };
static const u32c CROSS_COL[] = { RGBA(255, 255, 255, 255), RGBA(111, 207, 151, 255), RGBA(91, 214, 224, 255), RGBA(224, 128, 58, 255), RGBA(229, 72, 77, 255) };

/* ---------- Mapa ---------- */
typedef struct { float x0, x1, z0, z1, top; u32c col; } Box;
static Box boxes[MAXBOX];
static int nbox = 0;

static void addBox(float x, float z, float w, float d, float h, u32c c) {
    if (nbox >= MAXBOX) return;
    Box *b = &boxes[nbox++];
    b->x0 = x - w / 2; b->x1 = x + w / 2; b->z0 = z - d / 2; b->z1 = z + d / 2; b->top = h; b->col = c;
}

static void buildWorld(void) {
    const u32c WALL = RGBA(194, 171, 134, 255), WALL2 = RGBA(168, 150, 120, 255), CONC = RGBA(140, 146, 144, 255);
    const u32c CRATE = RGBA(142, 106, 67, 255), TEAL = RGBA(79, 122, 128, 255), RUST = RGBA(154, 90, 60, 255), OUT = RGBA(125, 116, 106, 255);
    nbox = 0;
    addBox(0, -HALF - 0.5f, HALF * 2 + 2, 1, 6, OUT); addBox(0, HALF + 0.5f, HALF * 2 + 2, 1, 6, OUT);
    addBox(-HALF - 0.5f, 0, 1, HALF * 2, 6, OUT); addBox(HALF + 0.5f, 0, 1, HALF * 2, 6, OUT);
    static const float bld[8][4] = { {-3.75f,-6,4.5f,.8f}, {3.75f,-6,4.5f,.8f}, {-3.75f,6,4.5f,.8f}, {3.75f,6,4.5f,.8f},
                                     {-6,-3.75f,.8f,4.5f}, {-6,3.75f,.8f,4.5f}, {6,-3.75f,.8f,4.5f}, {6,3.75f,.8f,4.5f} };
    for (int i = 0; i < 8; i++) addBox(bld[i][0], bld[i][1], bld[i][2], bld[i][3], 3.6f, CONC);
    addBox(0, 0, 2, 2, 1, CRATE); addBox(-2.6f, 2.6f, 1.4f, 1.4f, 2.2f, CRATE);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) {
        addBox(sx * 18, sz * 12, 9, .8f, 3.5f, WALL);
        addBox(sx * 13.6f, sz * 17.4f, .8f, 10, 3.5f, WALL2);
        addBox(sx * 8, sz * 14, 2, 2, 1, CRATE);
        addBox(sx * 9.4f, sz * 13.4f, 1.6f, 1.6f, 2.2f, CRATE);
        addBox(sx * 25.5f, sz * 6, 3, 3, 2.2f, CRATE);
        addBox(sx * 26, sz * 22, 2.2f, 2.2f, 2.2f, CRATE);
        addBox(sx * 3.8f, sz * 20, 4, 1, 1, CONC);
    }
    for (int sx = -1; sx <= 1; sx += 2) {
        addBox(sx * 22, 0, 2.6f, 9, 2.6f, sx < 0 ? TEAL : RUST);
        addBox(sx * 22, -11, 2.6f, 6, 2.6f, sx < 0 ? RUST : TEAL);
        addBox(sx * 22, 11, 2.6f, 6, 2.6f, sx < 0 ? RUST : TEAL);
    }
    addBox(0, -25, 7, 1, 1, CONC); addBox(0, 25, 7, 1, 1, CONC);
}

#define STEP 0.55f
static float groundAt(float x, float z, float feet, float r) {
    float g = 0;
    for (int i = 0; i < nbox; i++) {
        Box *b = &boxes[i];
        if (b->top > g && b->top <= feet + STEP && x > b->x0 - r * .5f && x < b->x1 + r * .5f && z > b->z0 - r * .5f && z < b->z1 + r * .5f) g = b->top;
    }
    return g;
}
static int blockedAt(float x, float z, float feet, float r) {
    for (int i = 0; i < nbox; i++) {
        Box *b = &boxes[i];
        if (b->top > feet + STEP && x > b->x0 - r && x < b->x1 + r && z > b->z0 - r && z < b->z1 + r) return 1;
    }
    return 0;
}
static void moveBody(float *x, float *z, float y, float dx, float dz, float r, int *hit) {
    if (!blockedAt(*x + dx, *z, y, r)) *x += dx; else if (hit) *hit = 1;
    if (!blockedAt(*x, *z + dz, y, r)) *z += dz; else if (hit) *hit = 1;
}

static float rayBox(const float *o, const float *d, const float *mn, const float *mx) {
    float t0 = 0, t1 = 1e30f;
    for (int i = 0; i < 3; i++) {
        if (fabsf(d[i]) < 1e-9f) { if (o[i] < mn[i] || o[i] > mx[i]) return 1e30f; continue; }
        float a = (mn[i] - o[i]) / d[i], b = (mx[i] - o[i]) / d[i];
        if (a > b) { float t = a; a = b; b = t; }
        if (a > t0) t0 = a;
        if (b < t1) t1 = b;
        if (t0 > t1) return 1e30f;
    }
    return t0;
}
static float raySphere(const float *o, const float *d, const float *c, float r) {
    float l[3] = { c[0] - o[0], c[1] - o[1], c[2] - o[2] };
    float tc = l[0] * d[0] + l[1] * d[1] + l[2] * d[2];
    if (tc < 0) return 1e30f;
    float d2 = l[0] * l[0] + l[1] * l[1] + l[2] * l[2] - tc * tc;
    if (d2 > r * r) return 1e30f;
    return tc - sqrtf(r * r - d2);
}
static float wallDist(const float *o, const float *d, float max) {
    float best = max;
    for (int i = 0; i < nbox; i++) {
        float mn[3] = { boxes[i].x0, 0, boxes[i].z0 }, mx[3] = { boxes[i].x1, boxes[i].top, boxes[i].z1 };
        float t = rayBox(o, d, mn, mx);
        if (t < best) best = t;
    }
    if (d[1] < 0) { float t = -o[1] / d[1]; if (t > 0 && t < best) best = t; }
    return best;
}
static int hasLOS(const float *a, const float *b) {
    float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    float L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d[0] /= L; d[1] /= L; d[2] /= L;
    return wallDist(a, d, L) >= L - 0.05f;
}
static int freeSpot(float zmin, float zmax, float *ox, float *oz) {
    for (int i = 0; i < 80; i++) {
        float x = rndf(-HALF + 2, HALF - 2), z = rndf(zmin, zmax);
        if (!blockedAt(x, z, 0, 1.0f)) { *ox = x; *oz = z; return 1; }
    }
    *ox = 0; *oz = zmin; return 0;
}

/* ---------- Render 3D ---------- */
typedef struct { float x, y, z, r, g, b; } V;
static float camX, camY, camZ, camYaw, camPitch, camF;

static void toView(float wx, float wy, float wz, float *x, float *y, float *z) {
    float dx = wx - camX, dy = wy - camY, dz = wz - camZ;
    float cy = cosf(camYaw), sy = sinf(camYaw), cp = cosf(camPitch), sp = sinf(camPitch);
    float xr = dx * cy - dz * sy;
    float zh = -dx * sy - dz * cy;
    *x = xr; *z = zh * cp + dy * sp; *y = -zh * sp + dy * cp;
}

#define NEAR 0.12f
static void emitPoly(V *in, int n, int overlay) {
    V out[12]; int m = 0;
    for (int i = 0; i < n; i++) {
        V *a = &in[i], *b = &in[(i + 1) % n];
        int ain = a->z >= NEAR, bin = b->z >= NEAR;
        if (ain) out[m++] = *a;
        if (ain != bin) {
            float t = (NEAR - a->z) / (b->z - a->z);
            V c = { lerpf(a->x, b->x, t), lerpf(a->y, b->y, t), NEAR, lerpf(a->r, b->r, t), lerpf(a->g, b->g, t), lerpf(a->b, b->b, t) };
            out[m++] = c;
        }
    }
    if (m < 3) return;
    /* Proyeccion. Cada vertice: x, y de pantalla, z de profundidad (lineal en pantalla), r, g, b. */
    typedef struct { float x, y, z, r, g, b; } SV;
    SV a[24], b2[24]; int na = m;
    for (int i = 0; i < m; i++) {
        float iz = 1.0f / out[i].z;
        a[i].x = SW * 0.5f + out[i].x * iz * camF;
        a[i].y = SH * 0.5f - out[i].y * iz * camF;
        a[i].z = overlay ? 16776960.0f : 16777000.0f * 0.1f * iz;
        a[i].r = out[i].r; a[i].g = out[i].g; a[i].b = out[i].b;
    }
    /* Recorte 2D contra la pantalla con margen: la GS solo admite coordenadas acotadas. */
    const float M = 128.0f, lim[4] = { -M, SW + M, -M, SH + M };
    for (int e = 0; e < 4 && na >= 3; e++) {
        int nb = 0;
        for (int i = 0; i < na; i++) {
            SV *p = &a[i], *q = &a[(i + 1) % na];
            float pv = (e < 2) ? p->x : p->y, qv = (e < 2) ? q->x : q->y;
            int pin = (e & 1) ? pv <= lim[e] : pv >= lim[e];
            int qin = (e & 1) ? qv <= lim[e] : qv >= lim[e];
            if (pin) b2[nb++] = *p;
            if (pin != qin) {
                float t = (lim[e] - pv) / (qv - pv);
                SV c = { lerpf(p->x, q->x, t), lerpf(p->y, q->y, t), lerpf(p->z, q->z, t), lerpf(p->r, q->r, t), lerpf(p->g, q->g, t), lerpf(p->b, q->b, t) };
                if (e < 2) c.x = lim[e]; else c.y = lim[e];
                b2[nb++] = c;
            }
        }
        memcpy(a, b2, sizeof(SV) * nb); na = nb;
    }
    if (na < 3) return;
    int sz[24]; u32c sc[24];
    for (int i = 0; i < na; i++) {
        sz[i] = (int)clampf(a[i].z, 0, 16777000.0f);
        sc[i] = RGBA((int)clampf(a[i].r, 0, 255), (int)clampf(a[i].g, 0, 255), (int)clampf(a[i].b, 0, 255), 255);
    }
    for (int i = 1; i + 1 < na; i++)
        gfx_tri(a[0].x, a[0].y, sz[0], sc[0], a[i].x, a[i].y, sz[i], sc[i], a[i + 1].x, a[i + 1].y, sz[i + 1], sc[i + 1]);
}

static void fogColor(u32c base, float shade, float dist, float *r, float *g, float *b) {
    float f = clampf((dist - 28.0f) / 60.0f, 0, 1);
    *r = lerpf(CR(base) * shade, CR(SKY), f); *g = lerpf(CG(base) * shade, CG(SKY), f); *b = lerpf(CB(base) * shade, CB(SKY), f);
}

static const int FACE[6][4] = { {0,4,6,2}, {1,3,7,5}, {0,1,5,4}, {2,6,7,3}, {0,2,3,1}, {4,5,7,6} };
static const float FN[6][3] = { {-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };

/* Caja orientada en el mundo (giro en Y). */
static void obox(float cx, float cy, float cz, float hw, float hh, float hd, float yaw, u32c col) {
    float c[8][3], ys = sinf(yaw), yc = cosf(yaw);
    for (int i = 0; i < 8; i++) {
        float lx = (i & 1) ? hw : -hw, ly = (i & 2) ? hh : -hh, lz = (i & 4) ? hd : -hd;
        c[i][0] = cx + lx * yc + lz * ys; c[i][1] = cy + ly; c[i][2] = cz - lx * ys + lz * yc;
    }
    for (int f = 0; f < 6; f++) {
        float nx = FN[f][0] * yc + FN[f][2] * ys, ny = FN[f][1], nz = -FN[f][0] * ys + FN[f][2] * yc;
        float fx = cx + nx * (fabsf(FN[f][0]) * hw + fabsf(FN[f][2]) * hd) , fy = cy + ny * hh, fz = cz + nz * (fabsf(FN[f][0]) * hw + fabsf(FN[f][2]) * hd);
        if (ny != 0) { fx = cx; fz = cz; }
        float vx = camX - fx, vy = camY - fy, vz = camZ - fz;
        if (nx * vx + ny * vy + nz * vz <= 0) continue;
        float dist = sqrtf(vx * vx + vy * vy + vz * vz);
        float lit = nx * -0.35f + ny * 0.8f + nz * 0.45f;
        float shade = clampf(0.62f + 0.38f * lit, 0.45f, 1.05f);
        V v[4];
        for (int k = 0; k < 4; k++) {
            float *p = c[FACE[f][k]];
            toView(p[0], p[1], p[2], &v[k].x, &v[k].y, &v[k].z);
            fogColor(col, shade, dist, &v[k].r, &v[k].g, &v[k].b);
        }
        emitPoly(v, 4, 0);
    }
}

/* Caja en espacio de camara, siempre por delante del mundo (arma en primera persona). */
static void vbox(float cx, float cy, float cz, float hw, float hh, float hd, u32c col) {
    for (int f = 0; f < 6; f++) {
        float nx = FN[f][0], ny = FN[f][1], nz = FN[f][2];
        float fx = cx + nx * hw, fy = cy + ny * hh, fz = cz + nz * hd;
        if (nx * -fx + ny * -fy + nz * -fz <= 0) continue;
        float shade = clampf(0.7f + 0.3f * (ny * 0.8f - nx * 0.35f), 0.45f, 1.0f);
        V v[4];
        for (int k = 0; k < 4; k++) {
            int i = FACE[f][k];
            v[k].x = cx + ((i & 1) ? hw : -hw); v[k].y = cy + ((i & 2) ? hh : -hh); v[k].z = cz + ((i & 4) ? hd : -hd);
            v[k].r = CR(col) * shade; v[k].g = CG(col) * shade; v[k].b = CB(col) * shade;
        }
        emitPoly(v, 4, 1);
    }
}

static void drawFloor(void) {
    for (int i = 0; i < 12; i++) for (int j = 0; j < 12; j++) {
        float x0 = -HALF + i * 5, z0 = -HALF + j * 5;
        float mx = x0 + 2.5f - camX, mz = z0 + 2.5f - camZ;
        float dist = sqrtf(mx * mx + mz * mz);
        u32c base = ((i + j) & 1) ? RGBA(179, 163, 132, 255) : RGBA(166, 150, 120, 255);
        V v[4]; float px[4] = { x0, x0 + 5, x0 + 5, x0 }, pz[4] = { z0, z0, z0 + 5, z0 + 5 };
        for (int k = 0; k < 4; k++) {
            toView(px[k], 0, pz[k], &v[k].x, &v[k].y, &v[k].z);
            fogColor(base, 1.0f, dist, &v[k].r, &v[k].g, &v[k].b);
        }
        emitPoly(v, 4, 0);
    }
}

/* ---------- Texto (fuente 5x7) ---------- */
static const unsigned char FONT[59][5] = {
    {0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0,8,7,3,0},
    {0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x2A,0x1C,0x7F,0x1C,0x2A},{8,8,0x3E,8,8},{0,0x80,0x70,0x30,0},{8,8,8,8,8},{0,0,0x60,0x60,0},{0x20,0x10,8,4,2},
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,9,7},
    {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0,0,0x14,0,0},{0,0x40,0x34,0,0},{0,8,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x59,9,6},
    {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x41,0x51,0x73},
    {0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x1C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},{3,1,0x7F,1,3},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,8,0x14,0x63},{3,4,0x78,4,3},{0x61,0x59,0x49,0x4D,0x43}
};
static float textW(const char *s, float sc) { return (float)strlen(s) * 6 * sc; }
static void drawText(float x, float y, float sc, const char *s, u32c col) {
    for (; *s; s++, x += 6 * sc) {
        int ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 90) continue;
        const unsigned char *g = FONT[ch - 32];
        for (int c = 0; c < 5; c++) {
            int r = 0;
            while (r < 7) {
                if (!((g[c] >> r) & 1)) { r++; continue; }
                int r0 = r; while (r < 7 && ((g[c] >> r) & 1)) r++;
                gfx_rect(x + c * sc, y + r0 * sc, x + (c + 1) * sc, y + r * sc, col);
            }
        }
    }
}
static void drawTextC(float cx, float y, float sc, const char *s, u32c col) { drawText(cx - textW(s, sc) * 0.5f + sc * 0.5f, y, sc, s, col); }

/* ---------- Armas ---------- */
typedef struct { const char *name; float dmg, rpm; int autof, mag, reserve; float spread, recoil; int pellets; float reload; } Weapon;
static const Weapon W[3] = {
    { "PISTOLA", 26, 380, 0, 12, 60, 0.004f, 0.016f, 1, 1.3f },
    { "RIFLE", 22, 640, 1, 30, 90, 0.008f, 0.013f, 1, 2.0f },
    { "ESCOPETA", 12, 80, 0, 6, 24, 0.05f, 0.05f, 8, 2.4f }
};

/* ---------- Estado de juego ---------- */
enum { M_MENU, M_SETTINGS, M_PLAYING, M_ROUNDEND, M_PAUSE, M_END };
typedef struct { float x, y, z, vx, vy, vz, yaw, pitch, hp; int alive, ground; float noise, bob; } Player;
typedef struct { float x, z, yaw, hp; int alive, state, burst; float react, fireCD, pause, strafe, strafeT, wx, wz, lkx, lkz, lost, timer, dead; float ph, amp, lastSin; } Bot;

static Player P;
static Bot bots[MAXBOT];
static int nbots = 0;
static int mode = M_MENU, prevMode = M_PLAYING, sel = 0, settingsFrom = M_MENU;
static int wsel, ammoMag[3], ammoRes[3];
static float wCD, wReload, wSwap, wKick, wFlash, freeze, afterT, orbit, hitFlash, hmT, hurtT, bannerT;
static int scoreMe, scoreBots, roundNo, kills, heads, deaths, firedLatch;
static float damageTaken;
static char bannerA[48], bannerB[48];
static char feed[4][40]; static float feedT[4];

enum { BS_PATROL, BS_SEARCH, BS_ALERT, BS_ENGAGE };

static int aliveBots(void) { int n = 0; for (int i = 0; i < nbots; i++) n += bots[i].alive; return n; }
static void setBanner(const char *a, const char *b, float t) { strncpy(bannerA, a, 47); strncpy(bannerB, b, 47); bannerA[47] = bannerB[47] = 0; bannerT = t; }
static void pushFeed(const char *s) {
    for (int i = 3; i > 0; i--) { memcpy(feed[i], feed[i - 1], 40); feedT[i] = feedT[i - 1]; }
    strncpy(feed[0], s, 39); feed[0][39] = 0; feedT[0] = 4.0f;
}

static void spawnBots(void) {
    nbots = S.bots > MAXBOT ? MAXBOT : S.bots;
    for (int i = 0; i < nbots; i++) {
        Bot *b = &bots[i]; memset(b, 0, sizeof(*b));
        freeSpot(-HALF + 3, -4, &b->x, &b->z);
        b->hp = DIFF_HP[S.diff]; b->alive = 1; b->state = BS_PATROL; b->yaw = rndf(0, 6.28f);
        b->strafe = (rand() & 1) ? 1 : -1; b->strafeT = 1; b->fireCD = 0.5f;
        freeSpot(-HALF + 2, HALF - 2, &b->wx, &b->wz);
    }
}
static void resetAmmo(void) {
    for (int i = 0; i < 3; i++) { ammoMag[i] = W[i].mag; ammoRes[i] = W[i].reserve; }
    wsel = 0; wCD = wReload = wSwap = wKick = wFlash = 0; firedLatch = 0;
}
static void startRound(void) {
    spawnBots();
    memset(&P, 0, sizeof(P)); P.z = 26; P.hp = 100; P.alive = 1; P.ground = 1;
    resetAmmo(); freeze = 2.4f; afterT = 0; mode = M_PLAYING; hurtT = 0;
    char b[40]; snprintf(b, 40, "RONDA %d", roundNo);
    setBanner(b, "ELIMINA A TODOS LOS BOTS", 2.2f);
}
static void startMatch(void) {
    scoreMe = scoreBots = 0; roundNo = 1; kills = heads = deaths = 0; damageTaken = 0;
    memset(feed, 0, sizeof(feed)); memset(feedT, 0, sizeof(feedT));
    startRound();
}
static void endRound(int won) {
    mode = M_ROUNDEND; afterT = 3.0f;
    if (won) { scoreMe++; setBanner("RONDA GANADA", "ELIMINASTE A TODOS LOS BOTS", 2.8f); audio_play(SFX_WIN, 1.0f); }
    else { scoreBots++; setBanner("RONDA PERDIDA", "TE ELIMINARON", 2.8f); audio_play(SFX_LOSE, 1.0f); }
}

static int hitSndLatch = 0;
static void damageBot(Bot *b, float dmg, int head) {
    if (!b->alive) return;
    b->hp -= dmg;
    if (b->hp > 0 && !hitSndLatch) { audio_play(SFX_HITMARK, 0.7f); hitSndLatch = 1; }
    if (b->state == BS_PATROL || b->state == BS_SEARCH) { b->state = BS_ALERT; b->react = DIFF_REACT[S.diff] * 0.5f; b->lkx = P.x; b->lkz = P.z; }
    if (b->hp <= 0) {
        b->alive = 0; b->dead = 0; kills++; if (head) heads++;
        { float ddx = P.x - b->x, ddz = P.z - b->z, dd = sqrtf(ddx * ddx + ddz * ddz);
          audio_play(SFX_DIE, clampf(1.1f - dd / 40.0f, 0.35f, 1.0f)); if (head) audio_play(SFX_HEAD, 0.8f); }
        pushFeed(head ? "BAJA - CABEZA" : "BAJA");
        if (aliveBots() == 0 && mode == M_PLAYING) endRound(1);
    }
}
static void hurtPlayer(float d) {
    if (!P.alive) return;
    P.hp -= d; damageTaken += d; hurtT = 0.25f; audio_play(SFX_HURT, 0.9f);
    if (P.hp <= 0) { P.hp = 0; P.alive = 0; deaths++; pushFeed("TE ELIMINARON"); if (mode == M_PLAYING) endRound(0); }
}

static void startReload(void) {
    if (wReload > 0 || ammoMag[wsel] >= W[wsel].mag || ammoRes[wsel] <= 0 || wSwap > 0) return;
    wReload = W[wsel].reload; audio_play(SFX_RELOAD, 0.8f);
}
static void switchTo(int i) {
    i = (i + 3) % 3;
    if (i == wsel || !P.alive) return;
    wsel = i; wReload = 0; wSwap = 0.35f; audio_play(SFX_SWITCH, 0.7f);
}

static void fireWeapon(void) {
    const Weapon *w = &W[wsel];
    if (!P.alive || wCD > 0 || wReload > 0 || wSwap > 0) return;
    if (ammoMag[wsel] <= 0) { wCD = 0.25f; audio_play(SFX_EMPTY, 0.6f); if (ammoRes[wsel] > 0) startReload(); return; }
    audio_play(wsel == 0 ? SFX_PISTOL : (wsel == 1 ? SFX_RIFLE : SFX_SHOTGUN), 1.0f);
    hitSndLatch = 0;
    ammoMag[wsel]--; wCD = 60.0f / w->rpm; wKick = 1; wFlash = 0.05f; P.noise = 1; firedLatch = 1;
    float spd = sqrtf(P.vx * P.vx + P.vz * P.vz);
    float spread = w->spread * (1 + clampf(spd / 5, 0, 1) * 2.2f + (P.ground ? 0 : 3));
    float cp = cosf(P.pitch), sp = sinf(P.pitch);
    float f[3] = { -sinf(P.yaw) * cp, sp, -cosf(P.yaw) * cp };
    float r[3] = { cosf(P.yaw), 0, -sinf(P.yaw) };
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    float o[3] = { P.x, P.y + 1.62f, P.z };
    int anyHit = 0, anyHead = 0;
    for (int p = 0; p < w->pellets; p++) {
        float sx = rndf(-1, 1) * spread, sy = rndf(-1, 1) * spread, d[3];
        for (int k = 0; k < 3; k++) d[k] = f[k] + r[k] * sx + u[k] * sy;
        float L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); d[0] /= L; d[1] /= L; d[2] /= L;
        float tBest = wallDist(o, d, 100); Bot *hit = NULL; int head = 0;
        for (int i = 0; i < nbots; i++) {
            Bot *b = &bots[i]; if (!b->alive) continue;
            float mn[3] = { b->x - .32f, 0, b->z - .32f }, mx[3] = { b->x + .32f, 1.42f, b->z + .32f }, c[3] = { b->x, 1.6f, b->z };
            float tb = rayBox(o, d, mn, mx), th = raySphere(o, d, c, .22f), t = tb < th ? tb : th;
            if (t < tBest) { tBest = t; hit = b; head = th <= tb; }
        }
        if (hit) {
            float dmg = w->dmg * (head ? 2.5f : 1.0f);
            if (w->pellets > 1) dmg *= clampf(1.15f - tBest / 35, 0.25f, 1);
            damageBot(hit, dmg, head); anyHit = 1; anyHead |= head;
        }
    }
    if (anyHit) { hitFlash = 0.12f; hmT = anyHead ? 1 : 0.5f; }
    P.pitch += w->recoil * rndf(0.7f, 1.3f); P.yaw += rndf(-0.5f, 0.5f) * w->recoil;
    if (ammoMag[wsel] == 0 && ammoRes[wsel] > 0) startReload();
}

static void botShoot(Bot *b, float dist) {
    audio_play(SFX_ESHOT, clampf(1.0f - dist / 45.0f, 0.15f, 0.9f));
    float o[3] = { b->x - sinf(b->yaw) * 0.6f, 1.35f, b->z - cosf(b->yaw) * 0.6f };
    float k = DIFF_AIM[S.diff];
    float t[3] = { P.x + rndf(-1, 1) * k * (1 + dist * .35f), P.y + 1.15f + rndf(-1, 1) * k * (1 + dist * .3f), P.z + rndf(-1, 1) * k * (1 + dist * .35f) };
    float d[3] = { t[0] - o[0], t[1] - o[1], t[2] - o[2] };
    float L = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); d[0] /= L; d[1] /= L; d[2] /= L;
    float wd = wallDist(o, d, 80);
    float mn[3] = { P.x - .3f, P.y, P.z - .3f }, mx[3] = { P.x + .3f, P.y + 1.75f, P.z + .3f };
    if (rayBox(o, d, mn, mx) < wd) hurtPlayer(DIFF_DMG[S.diff] * rndf(0.8f, 1.2f));
}

static void updateBots(float dt, int active) {
    int di = S.diff; float ex = P.x, ey = P.y + 1.37f, ez = P.z;
    for (int i = 0; i < nbots; i++) {
        Bot *b = &bots[i];
        if (!b->alive) { b->dead += dt; continue; }
        float dx = P.x - b->x, dz = P.z - b->z, dist = sqrtf(dx * dx + dz * dz);
        float toYaw = atan2f(-dx, -dz), dyaw = toYaw - b->yaw;
        dyaw = atan2f(sinf(dyaw), cosf(dyaw));
        float from[3] = { b->x, 1.55f, b->z }, to[3] = { ex, ey, ez };
        int sees = active && P.alive && dist < 48 && fabsf(dyaw) < 1.7f && hasLOS(from, to);
        if (!sees && active && P.alive && dist < 20 * P.noise) { b->lkx = P.x; b->lkz = P.z; if (b->state == BS_PATROL) b->state = BS_SEARCH; }
        if (sees) {
            b->lkx = P.x; b->lkz = P.z; b->lost = 0;
            if (b->state == BS_PATROL || b->state == BS_SEARCH) { b->state = BS_ALERT; b->react = DIFF_REACT[di] * rndf(0.8f, 1.3f); }
        } else if (b->state == BS_ENGAGE || b->state == BS_ALERT) { b->lost += dt; if (b->lost > 2) { b->state = BS_SEARCH; b->lost = 0; } }
        if (!active || !P.alive) b->state = BS_PATROL;
        float mx = 0, mz = 0, speed = 0, face = 1e9f;
        if (b->state == BS_PATROL) {
            float wx = b->wx - b->x, wz = b->wz - b->z, L = sqrtf(wx * wx + wz * wz);
            b->timer += dt;
            if (L < 1.2f || b->timer > 9) { if (rand() % 5 < 2) { b->wx = P.x + rndf(-8, 8); b->wz = P.z + rndf(-8, 8); } else freeSpot(-HALF + 2, HALF - 2, &b->wx, &b->wz); b->timer = 0; }
            if (L < 1e-3f) L = 1; mx = wx / L; mz = wz / L; speed = DIFF_SPEED[di] * 0.55f; face = atan2f(-wx, -wz);
        } else if (b->state == BS_SEARCH) {
            float wx = b->lkx - b->x, wz = b->lkz - b->z, L = sqrtf(wx * wx + wz * wz);
            if (L < 1.5f) { b->timer += dt; if (b->timer > 1.2f) { b->state = BS_PATROL; b->timer = 0; freeSpot(-HALF + 2, HALF - 2, &b->wx, &b->wz); } }
            else { mx = wx / L; mz = wz / L; speed = DIFF_SPEED[di] * 0.8f; face = atan2f(-wx, -wz); }
        } else if (b->state == BS_ALERT) {
            face = toYaw; b->react -= dt;
            if (b->react <= 0 && sees) { b->state = BS_ENGAGE; b->fireCD = rndf(0.1f, 0.3f); b->burst = 3 + rand() % 3; }
        } else {
            face = toYaw; b->strafeT -= dt;
            if (b->strafeT <= 0) { b->strafe = -b->strafe; b->strafeT = rndf(0.7f, 1.6f); }
            float rx = cosf(b->yaw), rz = -sinf(b->yaw), nx = dx / (dist + 1e-4f), nz = dz / (dist + 1e-4f);
            float adv = dist > 16 ? 1 : (dist < 6 ? -0.8f : 0);
            mx = rx * b->strafe * 0.8f + nx * adv; mz = rz * b->strafe * 0.8f + nz * adv; speed = DIFF_SPEED[di] * 0.7f;
            b->fireCD -= dt; if (b->pause > 0) b->pause -= dt;
            if (sees && b->fireCD <= 0 && b->pause <= 0 && fabsf(dyaw) < 0.5f) {
                b->fireCD = DIFF_FIRE[di] * rndf(0.8f, 1.2f);
                if (--b->burst <= 0) { b->pause = rndf(0.5f, 1.2f); b->burst = 3 + rand() % 3; }
                botShoot(b, dist);
            }
        }
        if (face < 1e8f) { float d2 = atan2f(sinf(face - b->yaw), cosf(face - b->yaw)); b->yaw += clampf(d2, -dt * 6, dt * 6); }
        float ml = sqrtf(mx * mx + mz * mz);
        if (ml > 0.01f && speed > 0) {
            int hit = 0; float ox0 = b->x, oz0 = b->z;
            moveBody(&b->x, &b->z, 0, mx / ml * speed * dt, mz / ml * speed * dt, 0.38f, &hit);
            { float mv = sqrtf((b->x - ox0) * (b->x - ox0) + (b->z - oz0) * (b->z - oz0));
              if (mv > 0.0005f) {
                  float sn;
                  b->ph += mv * 4.2f; sn = sinf(b->ph);
                  if (sn * b->lastSin < 0.0f && dist < 30.0f) audio_play(SFX_STEP, clampf(0.55f - dist / 55.0f, 0.06f, 0.5f));
                  b->lastSin = sn; b->amp = fminf(1.0f, b->amp + dt * 8.0f);
              } }
            if (hit) { b->lost += 0; b->strafe = -b->strafe; if (b->state == BS_PATROL) freeSpot(-HALF + 2, HALF - 2, &b->wx, &b->wz); }
        }
        if (!(ml > 0.01f && speed > 0)) b->amp = fmaxf(0.0f, b->amp - dt * 6.0f);
        b->x = clampf(b->x, -HALF + .5f, HALF - .5f); b->z = clampf(b->z, -HALF + .5f, HALF - .5f);
    }
}

static void updatePlayer(const Pad *pd, float dt, int canAct) {
    float ix = canAct ? pd->lx : 0, iz = canAct ? -pd->ly : 0;
    float il = sqrtf(ix * ix + iz * iz); if (il > 1) { ix /= il; iz /= il; }
    int walk = (pd->down & B_CIRCLE) != 0;
    float sp = walk ? 2.6f : 5.4f;
    float fx = -sinf(P.yaw), fz = -cosf(P.yaw), rx = cosf(P.yaw), rz = -sinf(P.yaw);
    float tvx = (fx * iz + rx * ix) * sp, tvz = (fz * iz + rz * ix) * sp;
    float k = (P.ground ? 14.0f : 2.5f) * dt; if (k > 1) k = 1;
    P.vx = lerpf(P.vx, tvx, k); P.vz = lerpf(P.vz, tvz, k);
    if (canAct && P.alive && (pd->pressed & B_CROSS) && P.ground) { P.vy = 6.6f; P.ground = 0; audio_play(SFX_JUMP, 0.6f); }
    { static int wasAir = 0; if (wasAir && P.ground) audio_play(SFX_LAND, 0.6f); wasAir = !P.ground; }
    moveBody(&P.x, &P.z, P.y, P.vx * dt, P.vz * dt, 0.38f, NULL);
    P.vy -= 20 * dt; P.y += P.vy * dt;
    float g = groundAt(P.x, P.z, P.y, 0.38f);
    if (P.y <= g) { P.y = g; P.vy = 0; P.ground = 1; } else if (P.y > g + 0.03f) P.ground = 0;
    P.x = clampf(P.x, -HALF + .4f, HALF - .4f); P.z = clampf(P.z, -HALF + .4f, HALF - .4f);
    float spd = sqrtf(P.vx * P.vx + P.vz * P.vz);
    if (P.ground && spd > 1.2f) {
        static float stepAcc = 0;
        P.bob += spd * dt * 1.7f; if (!walk) P.noise = fmaxf(P.noise, 0.6f);
        if (!walk && P.alive) { stepAcc += spd * dt; if (stepAcc > 2.3f) { stepAcc = 0; audio_play(SFX_STEP, 0.35f); } }
    }
    P.noise = fmaxf(0, P.noise - dt * 1.5f);
    if (P.alive && canAct) {
        float sens = 0.4f + S.sens * 0.28f;
        float lx = pd->rx * fabsf(pd->rx) * 0.5f + pd->rx * 0.5f, ly = pd->ry * fabsf(pd->ry) * 0.5f + pd->ry * 0.5f;
        P.yaw -= lx * sens * 2.6f * dt;
        P.pitch -= ly * sens * 1.8f * dt * (S.invertY ? -1 : 1);
    }
    P.pitch = clampf(P.pitch, -1.45f, 1.45f);
}

static void updateWeapon(const Pad *pd, float dt, int canAct) {
    if (wCD > 0) wCD -= dt;
    if (wSwap > 0) wSwap -= dt;
    if (wReload > 0) { wReload -= dt; if (wReload <= 0) { wReload = 0; int n = W[wsel].mag - ammoMag[wsel]; if (n > ammoRes[wsel]) n = ammoRes[wsel]; ammoMag[wsel] += n; ammoRes[wsel] -= n; } }
    wKick = fmaxf(0, wKick - dt * 9); wFlash = fmaxf(0, wFlash - dt);
    if (!canAct || !P.alive) return;
    int trig = (pd->down & (B_R2 | B_R1)) != 0;
    if (!trig) firedLatch = 0;
    if (trig && (W[wsel].autof || !firedLatch)) fireWeapon();
    if (pd->pressed & B_SQUARE) startReload();
    if (pd->pressed & B_TRIANGLE) switchTo(wsel + 1);
    if (pd->pressed & (B_L1 | B_L2)) switchTo(wsel - 1);
}

/* ---------- Dibujo de escena y HUD ---------- */
static void setCamera(float x, float y, float z, float yaw, float pitch) {
    camX = x; camY = y; camZ = z; camYaw = yaw; camPitch = pitch;
    camF = (SW * 0.5f) / tanf(S.fov * 0.5f * PI / 180.0f);
}
static void botPart(const Bot *b, float lx, float ly, float lz, float hw, float hh, float hd, u32c c) {
    float s = sinf(b->yaw), co = cosf(b->yaw);
    obox(b->x + lx * co + lz * s, ly, b->z - lx * s + lz * co, hw, hh, hd, b->yaw, c);
}
static void drawBots(void) {
    for (int i = 0; i < nbots; i++) {
        const Bot *b = &bots[i];
        if (!b->alive) {
            if (b->dead > 0.4f) obox(b->x, 0.12f, b->z, .5f, .12f, .3f, b->yaw, RGBA(120, 45, 40, 255));
            else { float h = 1.0f - b->dead * 2.0f; obox(b->x, 0.7f * h, b->z, .3f, .7f * h, .17f, b->yaw, RGBA(166, 61, 54, 255)); }
            continue;
        }
        {
            float sn = sinf(b->ph), amp = b->amp;
            float sw2 = sn * 0.26f * amp, liftL = fmaxf(0.0f, sn) * 0.17f * amp, liftR = fmaxf(0.0f, -sn) * 0.17f * amp;
            float bob = fabsf(sn) * 0.03f * amp;
            obox(b->x, 0.015f, b->z, .42f, .015f, .42f, b->yaw, RGBA(86, 76, 62, 255));          /* sombra en el piso */
            botPart(b, -.14f, (.76f + liftL) * .5f, -sw2, .1f, (.76f - liftL) * .5f, .11f, RGBA(54, 60, 66, 255));   /* pierna izq */
            botPart(b, .14f, (.76f + liftR) * .5f, sw2, .1f, (.76f - liftR) * .5f, .11f, RGBA(54, 60, 66, 255));     /* pierna der */
            botPart(b, 0, 1.08f + bob, 0, .28f, .35f, .15f, RGBA(166, 61, 54, 255));
            botPart(b, 0, 1.6f + bob, 0, .18f, .18f, .18f, RGBA(217, 176, 140, 255));
            botPart(b, 0, 1.76f + bob, 0, .22f, .07f, .22f, RGBA(45, 51, 56, 255));
            botPart(b, -.36f, 1.08f + bob, sw2 * 0.7f, .07f, .28f, .07f, RGBA(141, 51, 45, 255));                   /* brazo libre */
        }
        botPart(b, .3f, 1.15f + fabsf(sinf(b->ph)) * 0.03f * b->amp, -.3f, .05f, .05f, .3f, RGBA(31, 35, 38, 255));
    }
}
static void drawWeaponModel(void) {
    float kick = wKick * 0.06f, swap = wSwap > 0 ? wSwap / 0.35f : 0;
    float rl = wReload > 0 ? sinf(clampf(1 - wReload / W[wsel].reload, 0, 1) * PI) : 0;
    float ox = 0.22f, oy = -0.22f - swap * 0.25f - rl * 0.12f, oz = 0.55f - kick;
    u32c D = RGBA(39, 44, 48, 255), M = RGBA(59, 67, 73, 255), A = RGBA(224, 128, 58, 255), WOOD = RGBA(107, 74, 43, 255);
    if (wsel == 0) { vbox(ox, oy, oz + .15f, .03f, .04f, .15f, D); vbox(ox, oy - .09f, oz, .025f, .065f, .035f, M); vbox(ox, oy + .05f, oz + .02f, .01f, .01f, .01f, A); }
    else if (wsel == 1) { vbox(ox, oy, oz + .2f, .035f, .05f, .25f, D); vbox(ox, oy + .015f, oz + .6f, .017f, .017f, .2f, M); vbox(ox, oy - .12f, oz + .1f, .025f, .075f, .04f, M); vbox(ox, oy - .01f, oz - .05f, .03f, .05f, .11f, M); }
    else { vbox(ox, oy + .01f, oz + .4f, .022f, .022f, .4f, D); vbox(ox, oy - .035f, oz + .35f, .018f, .018f, .35f, M); vbox(ox, oy - .03f, oz + .38f, .038f, .03f, .11f, WOOD); vbox(ox, oy - .03f, oz, .03f, .05f, .13f, WOOD); }
    if (wFlash > 0) vbox(ox, oy + .01f, oz + (wsel == 0 ? .34f : (wsel == 1 ? .82f : .85f)), .04f, .04f, .03f, RGBA(255, 200, 100, 255));
}
static void drawCross(void) {
    float cx = SW * 0.5f, cy = SH * 0.5f, t = SH / 448.0f, len = 7 * t, gap = 5 * t, th = 1.2f * t;
    u32c c = CROSS_COL[S.cross];
    gfx_rect(cx - gap - len, cy - th, cx - gap, cy + th, c); gfx_rect(cx + gap, cy - th, cx + gap + len, cy + th, c);
    gfx_rect(cx - th, cy - gap - len, cx + th, cy - gap, c); gfx_rect(cx - th, cy + gap, cx + th, cy + gap + len, c);
    if (hitFlash > 0) {
        u32c h = hmT > 0.9f ? RGBA(229, 72, 77, 255) : RGBA(255, 255, 255, 255); float a = 9 * t, b = 16 * t;
        gfx_rect(cx - b, cy - b, cx - a, cy - b + 2 * t, h); gfx_rect(cx + a, cy - b, cx + b, cy - b + 2 * t, h);
        gfx_rect(cx - b, cy + b - 2 * t, cx - a, cy + b, h); gfx_rect(cx + a, cy + b - 2 * t, cx + b, cy + b, h);
    }
}
static void drawHud(void) {
    float t = SH / 224.0f, mx = SW * 0.07f, my = SH * 0.08f; char buf[64];
    drawCross();
    if (hurtT > 0) { int a = (int)(hurtT / 0.25f * 120); float e = SH * 0.08f; u32c c = RGBA(229, 72, 77, a);
        gfx_rect(0, 0, SW, e, c); gfx_rect(0, SH - e, SW, SH, c); gfx_rect(0, e, e, SH - e, c); gfx_rect(SW - e, e, SW, SH - e, c); }
    /* vida */
    drawText(mx, SH - my - 14 * t - 6 * t, t * 0.8f, "VIDA", RGBA(143, 160, 166, 255));
    snprintf(buf, 64, "%d", (int)ceilf(P.hp));
    drawText(mx, SH - my - 12 * t, t * 1.6f, buf, RGBA(232, 236, 233, 255));
    float bw = SW * 0.2f; u32c hc = P.hp > 60 ? RGBA(111, 207, 151, 255) : (P.hp > 30 ? RGBA(224, 128, 58, 255) : RGBA(229, 72, 77, 255));
    gfx_rect(mx, SH - my + 2 * t, mx + bw, SH - my + 4 * t, RGBA(0, 0, 0, 160)); gfx_rect(mx, SH - my + 2 * t, mx + bw * P.hp / 100, SH - my + 4 * t, hc);
    /* municion */
    snprintf(buf, 64, "%d / %d", ammoMag[wsel], ammoRes[wsel]);
    drawText(SW - mx - textW(buf, t * 1.6f), SH - my - 12 * t, t * 1.6f, buf, wReload > 0 ? RGBA(224, 128, 58, 255) : RGBA(232, 236, 233, 255));
    drawText(SW - mx - textW(W[wsel].name, t * 0.8f), SH - my - 20 * t, t * 0.8f, wReload > 0 ? "RECARGANDO" : W[wsel].name, RGBA(143, 160, 166, 255));
    /* marcador */
    snprintf(buf, 64, "RONDA %d", roundNo); drawTextC(SW * 0.5f, my * 0.7f, t * 1.0f, buf, RGBA(232, 236, 233, 255));
    snprintf(buf, 64, "TU %d - %d BOTS", scoreMe, scoreBots); drawTextC(SW * 0.5f, my * 0.7f + 10 * t, t * 0.9f, buf, RGBA(91, 180, 214, 255));
    snprintf(buf, 64, "VIVOS %d / %d", aliveBots(), nbots); drawTextC(SW * 0.5f, my * 0.7f + 19 * t, t * 0.7f, buf, RGBA(143, 160, 166, 255));
    /* radar */
    float rs = SH * 0.17f, rx0 = mx, ry0 = my, rc = rs * 0.5f, R = 24.0f;
    gfx_rect(rx0, ry0, rx0 + rs, ry0 + rs, RGBA(8, 11, 13, 150));
    gfx_rect(rx0 + rc - 0.5f, ry0, rx0 + rc + 0.5f, ry0 + rs, RGBA(255, 255, 255, 50)); gfx_rect(rx0, ry0 + rc - 0.5f, rx0 + rs, ry0 + rc + 0.5f, RGBA(255, 255, 255, 50));
    float ca = cosf(P.yaw), sa = sinf(P.yaw);
    for (int i = 0; i < nbots; i++) {
        if (!bots[i].alive) continue;
        float dx = bots[i].x - P.x, dz = bots[i].z - P.z, px = (dx * ca - dz * sa) / R * rc, py = (dx * sa + dz * ca) / R * rc;
        if (fabsf(px) > rc - 3 || fabsf(py) > rc - 3) continue;
        gfx_rect(rx0 + rc + px - 2, ry0 + rc + py - 2, rx0 + rc + px + 2, ry0 + rc + py + 2, RGBA(229, 72, 77, 255));
    }
    gfx_rect(rx0 + rc - 2, ry0 + rc - 2, rx0 + rc + 2, ry0 + rc + 2, RGBA(91, 180, 214, 255));
    /* avisos de baja */
    for (int i = 0; i < 4; i++) if (feedT[i] > 0 && feed[i][0]) drawText(SW - mx - textW(feed[i], t * 0.8f), my + i * 9 * t, t * 0.8f, feed[i], RGBA(232, 236, 233, 255));
    if (bannerT > 0) { drawTextC(SW * 0.5f, SH * 0.3f, t * 2.0f, bannerA, RGBA(232, 236, 233, 255)); drawTextC(SW * 0.5f, SH * 0.3f + 18 * t, t * 0.9f, bannerB, RGBA(143, 160, 166, 255)); }
}

/* ---------- Menus ---------- */
typedef struct { const char *label; } Item;
static void drawMenuBg(void) { gfx_rect(0, 0, SW, SH, RGBA(8, 11, 13, 150)); }
static void drawList(const char **items, const char **vals, int n, float y0) {
    float t = SH / 224.0f, w = SW * 0.62f, x0 = (SW - w) * 0.5f, rh = 14 * t;
    float maxh = (SH * 0.89f - y0) / n - 3 * t; if (rh > maxh) rh = maxh;
    for (int i = 0; i < n; i++) {
        float y = y0 + i * (rh + 3 * t); int on = i == sel;
        gfx_rect(x0, y, x0 + w, y + rh, on ? RGBA(224, 128, 58, 255) : RGBA(19, 26, 31, 230));
        u32c tc = on ? RGBA(23, 16, 10, 255) : RGBA(232, 236, 233, 255);
        drawText(x0 + 5 * t, y + 3.5f * t, t * 1.0f, items[i], tc);
        if (vals && vals[i]) {
            char b[40]; snprintf(b, 40, on ? "< %s >" : "%s", vals[i]);
            drawText(x0 + w - 5 * t - textW(b, t), y + 3.5f * t, t, b, tc);
        }
    }
}
static void drawTitle(void) {
    float t = SH / 224.0f;
    drawTextC(SW * 0.5f, SH * 0.1f, t * 4.5f, "PUNTO CERO", RGBA(232, 236, 233, 255));
    gfx_rect(SW * 0.5f - 60 * t, SH * 0.1f + 36 * t, SW * 0.5f + 60 * t, SH * 0.1f + 38 * t, RGBA(224, 128, 58, 255));
    drawTextC(SW * 0.5f, SH * 0.1f + 42 * t, t * 0.9f, "TACTICO POR RONDAS - 1 CONTRA BOTS", RGBA(143, 160, 166, 255));
}
static char vbuf[10][24];
static void settingsLabels(const char **vals) {
    vals[0] = DIFF_NAMES[S.diff];
    snprintf(vbuf[1], 24, "%d", S.bots); vals[1] = vbuf[1];
    snprintf(vbuf[2], 24, "%d", S.rounds); vals[2] = vbuf[2];
    snprintf(vbuf[3], 24, "%d", S.sens); vals[3] = vbuf[3];
    snprintf(vbuf[4], 24, "%d", S.fov); vals[4] = vbuf[4];
    vals[5] = S.invertY ? "SI" : "NO"; vals[6] = CROSS_NAMES[S.cross];
    snprintf(vbuf[7], 24, "%d", S.musicV); vals[7] = vbuf[7];
    snprintf(vbuf[8], 24, "%d", S.sfxV); vals[8] = vbuf[8]; vals[9] = NULL;
}
static void changeSetting(int i, int dir) {
    switch (i) {
    case 0: S.diff = (S.diff + dir + 3) % 3; break;
    case 1: S.bots = (int)clampf((float)(S.bots + dir), 1, 8); break;
    case 2: { int k = 0; for (int j = 0; j < 4; j++) if (ROUND_OPTS[j] == S.rounds) k = j; S.rounds = ROUND_OPTS[(k + dir + 4) % 4]; break; }
    case 3: S.sens = (int)clampf((float)(S.sens + dir), 1, 10); break;
    case 4: S.fov = (int)clampf((float)(S.fov + dir * 5), 60, 100); break;
    case 5: S.invertY = !S.invertY; break;
    case 6: S.cross = (S.cross + dir + 5) % 5; break;
    case 7: S.musicV = (int)clampf((float)(S.musicV + dir), 0, 5); break;
    case 8: S.sfxV = (int)clampf((float)(S.sfxV + dir), 0, 5); break;
    }
    audio_set_volumes(S.musicV / 5.0f, S.sfxV / 5.0f);
}
static void menuNav(const Pad *pd, int n) {
    if (pd->pressed & B_UP) { sel = (sel + n - 1) % n; audio_play(SFX_MENU, 0.5f); }
    if (pd->pressed & B_DOWN) { sel = (sel + 1) % n; audio_play(SFX_MENU, 0.5f); }
}

void game_init(void) {
    srand(12345);
    buildWorld();
    mode = M_MENU; sel = 0; orbit = 0;
    audio_set_volumes(S.musicV / 5.0f, S.sfxV / 5.0f);
}

/* la musica baja mientras se juega */
float game_music_mul(void) { return mode == M_PLAYING ? 0.4f : (mode == M_ROUNDEND || mode == M_PAUSE ? 0.7f : 1.0f); }

void game_frame(const Pad *pd, float dt) {
    float t = SH / 224.0f;
    /* --- logica --- */
    if (hurtT > 0) hurtT -= dt;
    if (hitFlash > 0) hitFlash -= dt;
    if (bannerT > 0) bannerT -= dt;
    for (int i = 0; i < 4; i++) if (feedT[i] > 0) feedT[i] -= dt;
    int inGame = mode == M_PLAYING || mode == M_ROUNDEND || mode == M_PAUSE;
    if ((mode == M_MENU || mode == M_PAUSE || mode == M_END || mode == M_SETTINGS) && (pd->pressed & (B_CROSS | B_CIRCLE))) audio_play(SFX_SELECT, 0.5f);
    if (mode == M_PLAYING || mode == M_ROUNDEND) {
        int canAct = mode == M_PLAYING && freeze <= 0;
        if (mode == M_PLAYING && freeze > 0) freeze -= dt;
        updatePlayer(pd, dt, canAct);
        updateWeapon(pd, dt, canAct);
        updateBots(dt, canAct);
        if (mode == M_ROUNDEND) {
            afterT -= dt;
            if (afterT <= 0) {
                if (scoreMe >= S.rounds || scoreBots >= S.rounds) { mode = M_END; sel = 0; }
                else { roundNo++; startRound(); }
            }
        }
        if (mode == M_PLAYING && (pd->pressed & B_START)) { mode = M_PAUSE; sel = 0; prevMode = M_PLAYING; }
    } else if (mode == M_PAUSE) {
        menuNav(pd, 3);
        if ((pd->pressed & B_START) || (pd->pressed & B_CIRCLE)) mode = prevMode;
        else if (pd->pressed & B_CROSS) {
            if (sel == 0) mode = prevMode;
            else if (sel == 1) { settingsFrom = M_PAUSE; mode = M_SETTINGS; sel = 0; }
            else { mode = M_MENU; sel = 0; nbots = 0; }
        }
    } else if (mode == M_MENU) {
        orbit += dt * 0.08f; menuNav(pd, 2);
        if (pd->pressed & B_CROSS) { if (sel == 0) startMatch(); else { settingsFrom = M_MENU; mode = M_SETTINGS; sel = 0; } }
    } else if (mode == M_SETTINGS) {
        orbit += dt * 0.08f; menuNav(pd, 10);
        if (pd->pressed & B_LEFT) { changeSetting(sel, -1); audio_play(SFX_MENU, 0.5f); }
        if (pd->pressed & (B_RIGHT | (sel < 9 ? B_CROSS : 0))) { changeSetting(sel, 1); audio_play(SFX_MENU, 0.5f); }
        if (((pd->pressed & B_CROSS) && sel == 9) || (pd->pressed & B_CIRCLE)) { mode = settingsFrom; sel = 1; }
    } else if (mode == M_END) {
        orbit += dt * 0.08f; menuNav(pd, 2);
        if (pd->pressed & B_CROSS) { if (sel == 0) startMatch(); else { mode = M_MENU; sel = 0; nbots = 0; } }
    }
    /* --- dibujo --- */
    gfx_begin(SKY);
    if (inGame) {
        float bob = P.ground ? sinf(P.bob * 2) * 0.035f * clampf(sqrtf(P.vx * P.vx + P.vz * P.vz) / 5, 0, 1) : 0;
        setCamera(P.x, P.y + 1.62f + bob - (P.alive ? 0 : 1.1f), P.z, P.yaw, P.pitch);
    } else setCamera(cosf(orbit) * 22, 9 + sinf(orbit * 1.7f) * 2, sinf(orbit) * 22, 0, 0);
    if (!inGame) { /* camara orbital mirando al centro */
        float dx = -camX, dz = -camZ, dy = 1.5f - camY;
        camYaw = atan2f(dx, -dz) * -1.0f; camYaw = atan2f(-dx, -dz); camPitch = atan2f(dy, sqrtf(dx * dx + dz * dz));
    }
    drawFloor();
    for (int i = 0; i < nbox; i++) {
        Box *b = &boxes[i];
        obox((b->x0 + b->x1) * .5f, b->top * .5f, (b->z0 + b->z1) * .5f, (b->x1 - b->x0) * .5f, b->top * .5f, (b->z1 - b->z0) * .5f, 0, b->col);
    }
    drawBots();
    if (inGame && P.alive) drawWeaponModel();
    if (inGame && mode != M_PAUSE) drawHud();
    if (mode == M_MENU) {
        drawMenuBg(); drawTitle();
        const char *it[] = { "PARTIDA RAPIDA", "CONFIGURACION" };
        drawList(it, NULL, 2, SH * 0.52f);
        drawTextC(SW * 0.5f, SH * 0.9f - 6 * t, t * 0.7f, "ARRIBA/ABAJO MOVER   X ACEPTAR", RGBA(143, 160, 166, 255));
    } else if (mode == M_SETTINGS) {
        drawMenuBg(); drawTextC(SW * 0.5f, SH * 0.08f, t * 3.0f, "CONFIGURACION", RGBA(232, 236, 233, 255));
        const char *it[] = { "DIFICULTAD", "BOTS POR RONDA", "RONDAS PARA GANAR", "SENSIBILIDAD", "CAMPO DE VISION", "INVERTIR EJE Y", "COLOR DE MIRA", "VOLUMEN MUSICA", "VOLUMEN EFECTOS", "VOLVER" };
        const char *vals[10]; settingsLabels(vals);
        drawList(it, vals, 10, SH * 0.20f);
        drawTextC(SW * 0.5f, SH * 0.92f - 6 * t, t * 0.7f, "IZQ/DER CAMBIAR   O VOLVER", RGBA(143, 160, 166, 255));
    } else if (mode == M_PAUSE) {
        drawMenuBg(); drawTextC(SW * 0.5f, SH * 0.2f, t * 4.0f, "PAUSA", RGBA(232, 236, 233, 255));
        const char *it[] = { "CONTINUAR", "CONFIGURACION", "MENU PRINCIPAL" };
        drawList(it, NULL, 3, SH * 0.45f);
    } else if (mode == M_END) {
        int won = scoreMe >= S.rounds; char b[48];
        drawMenuBg(); drawTextC(SW * 0.5f, SH * 0.12f, t * 4.5f, won ? "VICTORIA" : "DERROTA", won ? RGBA(111, 207, 151, 255) : RGBA(229, 72, 77, 255));
        snprintf(b, 48, "MARCADOR %d - %d", scoreMe, scoreBots); drawTextC(SW * 0.5f, SH * 0.12f + 40 * t, t * 1.2f, b, RGBA(232, 236, 233, 255));
        snprintf(b, 48, "BAJAS %d   CABEZAS %d   MUERTES %d", kills, heads, deaths); drawTextC(SW * 0.5f, SH * 0.12f + 56 * t, t * 0.9f, b, RGBA(143, 160, 166, 255));
        snprintf(b, 48, "DANO RECIBIDO %d", (int)damageTaken); drawTextC(SW * 0.5f, SH * 0.12f + 68 * t, t * 0.9f, b, RGBA(143, 160, 166, 255));
        const char *it[] = { "REVANCHA", "MENU PRINCIPAL" };
        drawList(it, NULL, 2, SH * 0.6f);
    }
    gfx_end();
}
