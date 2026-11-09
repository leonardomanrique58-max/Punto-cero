/* Capa de plataforma: el juego solo habla con estas funciones.
 * ps2_platform.c las implementa con gsKit + libpad (consola real / PCSX2).
 * host_platform.c las implementa con un rasterizador por software (pruebas en PC). */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

typedef uint32_t u32c;
#define RGBA(r, g, b, a) ((((u32c)(a)) << 24) | (((u32c)(r)) << 16) | (((u32c)(g)) << 8) | ((u32c)(b)))
#define CR(c) (((c) >> 16) & 255)
#define CG(c) (((c) >> 8) & 255)
#define CB(c) ((c) & 255)
#define CA(c) (((c) >> 24) & 255)

/* Botones (mascara propia, igual en PS2 y en PC) */
enum {
    B_UP = 1 << 0, B_DOWN = 1 << 1, B_LEFT = 1 << 2, B_RIGHT = 1 << 3,
    B_CROSS = 1 << 4, B_CIRCLE = 1 << 5, B_SQUARE = 1 << 6, B_TRIANGLE = 1 << 7,
    B_L1 = 1 << 8, B_R1 = 1 << 9, B_L2 = 1 << 10, B_R2 = 1 << 11,
    B_START = 1 << 12, B_SELECT = 1 << 13, B_L3 = 1 << 14, B_R3 = 1 << 15
};
typedef struct {
    float lx, ly, rx, ry;      /* sticks, -1..1 (arriba = -1 en ly/ry) */
    unsigned down, pressed;    /* botones mantenidos / recien presionados */
} Pad;

extern int SW, SH;             /* resolucion del framebuffer */

void gfx_init(void);
void gfx_begin(u32c clear);
void gfx_end(void);
/* Triangulo con color por vertice; z mayor = mas cerca (0..16777215) */
void gfx_tri(float x1, float y1, int z1, u32c c1,
             float x2, float y2, int z2, u32c c2,
             float x3, float y3, int z3, u32c c3);
/* Rectangulo 2D siempre al frente; el alfa del color se respeta */
void gfx_rect(float x1, float y1, float x2, float y2, u32c c);
void pad_init(void);
void pad_poll(Pad *p);

/* Juego */
void game_init(void);
void game_frame(const Pad *p, float dt);
float game_music_mul(void);   /* 0..1: volumen relativo de la musica segun el estado */

#endif
