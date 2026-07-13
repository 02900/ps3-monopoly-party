/*
 * Minimal MikMod audio for the boilerplate — a looping tune + one SFX, all
 * SYNTHESIZED in code (no asset files, so it's license-safe to ship in a
 * template). Backend-independent: identical in the tiny3d / rsxgl / raylib repos.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Looping music tracks (synthesized), one per screen/phase. WIN is a one-shot. */
enum { MUS_TITLE, MUS_MENU, MUS_INGAME, MUS_AUCTION, MUS_JAIL, MUS_WIN, MUS_COUNT };
/* Short one-shot SFX. */
enum { SFX_ROLL, SFX_BUY, SFX_CASH, SFX_ERROR, SFX_COUNT };

void audio_init(void);      /* once at startup (after the PS3 stack is up)  */
void audio_update(void);    /* every frame — drives MikMod's software mixer */
void audio_shutdown(void);  /* on exit                                      */

void audio_music(int track);/* switch the looping music (idempotent: no-op if already on it) */
void audio_sfx(int id);     /* play a one-shot SFX over the music            */
void audio_play_blip(void); /* compat shim -> audio_sfx(SFX_ROLL)            */

#ifdef __cplusplus
}
#endif
