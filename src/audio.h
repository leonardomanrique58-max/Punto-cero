#ifndef AUDIO_H
#define AUDIO_H

enum {
    SFX_PISTOL, SFX_RIFLE, SFX_SHOTGUN, SFX_ESHOT, SFX_RELOAD, SFX_EMPTY,
    SFX_SWITCH, SFX_HITMARK, SFX_HEAD, SFX_DIE, SFX_HURT, SFX_STEP,
    SFX_JUMP, SFX_LAND, SFX_WIN, SFX_LOSE, SFX_MENU, SFX_SELECT,
    SFX_COUNT
};

void audio_init(int fps);                       /* carga audsrv y genera los sonidos */
void audio_set_volumes(float music, float sfx); /* 0.0 - 1.0 */
void audio_play(int id, float vol);             /* vol 0.0 - 1.0 */
void audio_frame(float musicMul);               /* llamar 1 vez por cuadro */

#endif
