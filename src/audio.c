/*
 * Audio de PUNTO CERO 2: todos los efectos y la musica se generan por codigo
 * (no hace falta ningun archivo de sonido). Usa audsrv (modulo IOP incrustado).
 */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdlib.h>
#include <math.h>
#include <audsrv.h>
#include "audio.h"

extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;

#define RATE     22050
#define NVOICES  12
#define TAU      6.2831853f

typedef struct { short *d; int n; } Sfx;
typedef struct { const short *d; int n, pos; float vol; } Voice;

static Sfx   sfx[SFX_COUNT];
static Voice voices[NVOICES];
static int   audio_ok = 0;
static int   fps = 60;
static float musicVol = 0.6f, sfxVol = 0.8f;
static short mixbuf[700];

static unsigned rngs = 22222u;
static float nrand(void)
{
    rngs ^= rngs << 13;
    rngs ^= rngs >> 17;
    rngs ^= rngs << 5;
    return ((rngs & 0xffff) / 32768.0f) - 1.0f;
}

static short clip16(float v)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (short)(v * 30000.0f);
}

static short *alloc_sfx(int id, float sec)
{
    sfx[id].n = (int)(sec * RATE);
    sfx[id].d = (short *)malloc(sfx[id].n * sizeof(short));
    if (!sfx[id].d) sfx[id].n = 0;
    return sfx[id].d;
}

/* Disparo generico: chasquido + cuerpo filtrado + golpe grave */
static void gen_gun(int id, float sec, float lpc, float crackDecay, float bodyDecay,
                    float f0, float f1, float thumpDecay,
                    float aCrack, float aBody, float aThump)
{
    int i;
    float t, ph = 0.0f, lp = 0.0f, nz, f, tail;
    short *d = alloc_sfx(id, sec);
    if (!d) return;
    for (i = 0; i < sfx[id].n; i++) {
        t = i / (float)RATE;
        nz = nrand();
        lp += lpc * (nz - lp);
        f = f0 + f1 * expf(-t * 20.0f);
        ph += f / RATE; if (ph >= 1.0f) ph -= 1.0f;
        tail = (sec - t) * 60.0f; if (tail > 1.0f) tail = 1.0f;
        d[i] = clip16(tail * (aCrack * nz * expf(-t * crackDecay)
                            + aBody * lp * expf(-t * bodyDecay)
                            + aThump * sinf(TAU * ph) * expf(-t * thumpDecay)));
    }
}

/* Tono con barrido de frecuencia. wave: 0 seno, 1 cuadrada, 2 sierra, 3 triangulo */
static void gen_tone(int id, float sec, float f0, float f1, int wave,
                     float decay, float noise, float amp)
{
    int i;
    float t, ph = 0.0f, f, w, env, tail;
    short *d = alloc_sfx(id, sec);
    if (!d) return;
    for (i = 0; i < sfx[id].n; i++) {
        t = i / (float)RATE;
        f = f0 + (f1 - f0) * (t / sec);
        ph += f / RATE; if (ph >= 1.0f) ph -= 1.0f;
        switch (wave) {
            case 0:  w = sinf(TAU * ph); break;
            case 1:  w = (ph < 0.5f) ? 1.0f : -1.0f; break;
            case 2:  w = 2.0f * ph - 1.0f; break;
            default: w = 2.0f * fabsf(2.0f * ph - 1.0f) - 1.0f; break;
        }
        env = expf(-t * decay);
        tail = (sec - t) * 60.0f; if (tail < 1.0f) env *= tail;
        d[i] = clip16((amp * w + noise * nrand()) * env);
    }
}

/* Secuencia corta de notas */
static void gen_jingle(int id, const float *notes, int n, float noteLen,
                       int wave, float decay, float amp)
{
    int i, k;
    float t, tt, p, w, env, tail, sec = n * noteLen + 0.2f;
    short *d = alloc_sfx(id, sec);
    if (!d) return;
    for (i = 0; i < sfx[id].n; i++) {
        t = i / (float)RATE;
        k = (int)(t / noteLen); if (k >= n) k = n - 1;
        tt = t - k * noteLen;
        p = tt * notes[k]; p -= (int)p;
        w = (wave == 1) ? ((p < 0.5f) ? 1.0f : -1.0f) : (2.0f * p - 1.0f);
        env = expf(-tt * decay);
        tail = (sec - t) * 60.0f; if (tail < 1.0f) env *= tail;
        d[i] = clip16(amp * w * env);
    }
}

/* Recarga: tres "clacks" metalicos */
static void gen_reload(void)
{
    static const float times[3] = { 0.0f, 0.42f, 0.72f };
    int i, k;
    float t, tt, s, p;
    short *d = alloc_sfx(SFX_RELOAD, 1.0f);
    if (!d) return;
    for (i = 0; i < sfx[SFX_RELOAD].n; i++) {
        t = i / (float)RATE;
        s = 0.0f;
        for (k = 0; k < 3; k++) {
            tt = t - times[k];
            if (tt >= 0.0f && tt < 0.08f) {
                p = tt * ((k == 1) ? 900.0f : 600.0f); p -= (int)p;
                s += (0.5f * nrand() + 0.35f * ((p < 0.5f) ? 1.0f : -1.0f)) * expf(-tt * 60.0f);
            }
        }
        d[i] = clip16(s * 0.7f);
    }
}

static void gen_all(void)
{
    static const float winN[4]  = { 523.25f, 659.25f, 783.99f, 1046.5f };
    static const float loseN[4] = { 392.0f, 329.63f, 261.63f, 196.0f };

    gen_gun(SFX_PISTOL,  0.20f, 0.45f, 110.0f, 22.0f,  60.0f, 140.0f, 14.0f, 0.6f, 0.8f, 0.6f);
    gen_gun(SFX_RIFLE,   0.18f, 0.50f, 130.0f, 26.0f,  70.0f, 120.0f, 16.0f, 0.7f, 0.7f, 0.5f);
    gen_gun(SFX_SHOTGUN, 0.45f, 0.30f,  60.0f, 10.0f,  40.0f, 120.0f,  8.0f, 0.7f, 1.0f, 0.9f);
    gen_gun(SFX_ESHOT,   0.17f, 0.20f, 120.0f, 20.0f,  70.0f,  60.0f, 16.0f, 0.35f, 0.9f, 0.4f);
    gen_gun(SFX_STEP,    0.06f, 0.08f,   1.0f, 55.0f,   0.0f,   0.0f,  1.0f, 0.0f, 3.0f, 0.0f);
    gen_reload();
    gen_tone(SFX_EMPTY,   0.05f, 1500.0f, 1100.0f, 1, 60.0f, 0.2f, 0.3f);
    gen_tone(SFX_SWITCH,  0.08f, 1200.0f,  800.0f, 1, 45.0f, 0.3f, 0.3f);
    gen_tone(SFX_HITMARK, 0.07f,  900.0f,  500.0f, 1, 40.0f, 0.1f, 0.35f);
    gen_tone(SFX_HEAD,    0.30f, 1760.0f, 1760.0f, 0, 12.0f, 0.0f, 0.5f);
    gen_tone(SFX_DIE,     0.50f,  300.0f,   50.0f, 2,  4.0f, 0.25f, 0.6f);
    gen_tone(SFX_HURT,    0.28f,  150.0f,   90.0f, 1,  8.0f, 0.35f, 0.5f);
    gen_tone(SFX_JUMP,    0.15f,  200.0f,  520.0f, 0,  6.0f, 0.0f, 0.5f);
    gen_tone(SFX_LAND,    0.10f,   90.0f,   50.0f, 0, 25.0f, 0.15f, 0.9f);
    gen_tone(SFX_MENU,    0.05f,  700.0f,  700.0f, 1, 40.0f, 0.0f, 0.25f);
    gen_tone(SFX_SELECT,  0.10f,  600.0f, 1000.0f, 1, 15.0f, 0.0f, 0.3f);
    gen_jingle(SFX_WIN,  winN,  4, 0.14f, 1, 6.0f, 0.3f);
    gen_jingle(SFX_LOSE, loseN, 4, 0.20f, 2, 4.0f, 0.3f);
}

void audio_play(int id, float vol)
{
    int i, best = 0, bestRem = 1 << 30;
    if (!audio_ok || id < 0 || id >= SFX_COUNT || !sfx[id].d || vol <= 0.01f) return;
    if (vol > 1.0f) vol = 1.0f;
    for (i = 0; i < NVOICES; i++) {
        int rem = voices[i].d ? voices[i].n - voices[i].pos : 0;
        if (rem <= 0) { best = i; break; }
        if (rem < bestRem) { bestRem = rem; best = i; }
    }
    voices[best].d = sfx[id].d;
    voices[best].n = sfx[id].n;
    voices[best].pos = 0;
    voices[best].vol = vol;
}

/* Musica: bajo + arpegio + bombo/caja/hi-hat, 120 bpm, Am - F - C - G */
#define STEP_LEN (RATE * 60 / (120 * 2))

static const int chordRoot[4]  = { 0, -4, 3, -2 };
static const int chordThird[4] = { 3, 4, 4, 4 };
static const int arpIdx[8]     = { 0, 1, 2, 1, 0, 1, 2, 1 };
static unsigned long mpos = 0;
static float bassF = 110.0f, leadF = 440.0f;
static float bph = 0.0f, lph = 0.0f, kph = 0.0f;

static float music_sample(void)
{
    unsigned long p = mpos++;
    int step = (int)((p / STEP_LEN) % 32);
    int inStep = (int)(p % STEP_LEN);
    int bar = step >> 3, s8 = step & 7;
    float env = 1.0f - (float)inStep / STEP_LEN;
    float out = 0.0f, tri, sq, kf, he;

    if (inStep == 0) {
        int tones[3];
        tones[0] = 0; tones[1] = chordThird[bar]; tones[2] = 7;
        bassF = 110.0f * powf(2.0f, chordRoot[bar] / 12.0f);
        leadF = 440.0f * powf(2.0f, (chordRoot[bar] + tones[arpIdx[s8]]) / 12.0f);
    }

    bph += bassF / RATE; if (bph >= 1.0f) bph -= 1.0f;
    tri = 2.0f * fabsf(2.0f * bph - 1.0f) - 1.0f;
    if (s8 != 3 && s8 != 7) out += 0.28f * tri * (0.4f + 0.6f * env);

    lph += leadF / RATE; if (lph >= 1.0f) lph -= 1.0f;
    sq = (lph < 0.5f) ? 1.0f : -1.0f;
    out += 0.07f * sq * env * env;

    if (s8 == 0 || s8 == 4) {
        kf = 45.0f + 110.0f * env * env * env;
        kph += kf / RATE; if (kph >= 1.0f) kph -= 1.0f;
        out += 0.4f * sinf(TAU * kph) * env * env;
    }
    if (s8 == 2 || s8 == 6)
        out += 0.12f * nrand() * env * env;
    if (s8 & 1) {
        he = 1.0f - inStep / (STEP_LEN * 0.25f);
        if (he > 0.0f) out += 0.07f * nrand() * he * he;
    }
    return out;
}

void audio_set_volumes(float music, float sfxv)
{
    musicVol = music;
    sfxVol = sfxv;
}

void audio_init(int fps_)
{
    int ret;
    audsrv_fmt_t fmt;

    fps = fps_;
    SifLoadModule("rom0:LIBSD", 0, NULL);
    SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &ret);
    if (audsrv_init() != 0) return;          /* sin audio: el juego sigue igual */

    fmt.bits = 16;
    fmt.freq = RATE;
    fmt.channels = 1;
    audsrv_set_format(&fmt);
    audsrv_set_volume(MAX_VOLUME);

    gen_all();
    audio_ok = 1;
}

void audio_frame(float musicMul)
{
    int i, v, n;
    float mv = musicVol * musicMul * 0.5f;
    if (!audio_ok) return;

    n = RATE / fps + 1;
    for (i = 0; i < n; i++) {
        float s = (mv > 0.001f) ? music_sample() * mv : 0.0f;
        for (v = 0; v < NVOICES; v++) {
            if (voices[v].d && voices[v].pos < voices[v].n)
                s += voices[v].d[voices[v].pos++] * (1.0f / 32768.0f) * voices[v].vol * sfxVol * 0.9f;
        }
        mixbuf[i] = clip16(s);
    }
    audsrv_wait_audio(n * 2);
    audsrv_play_audio((char *)mixbuf, n * 2);
}
