/*
 * Minimal MikMod audio — a looping tune + a "blip" SFX, both SYNTHESIZED as
 * 16-bit mono PCM in code and handed to MikMod through an in-memory MREADER.
 * No audio assets on disk, so this is safe to ship in a template. Init is
 * defensive: any failure leaves audio_ok = 0 and every call becomes a no-op.
 *
 * (Adapted from ps3-mega-mario/source/audio.c, trimmed to two original sounds.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ppu-types.h>
#include <mikmod.h>

#include "audio.h"

#define SR 22050   /* synthesis sample rate */

static int     audio_ok = 0;
static SAMPLE *s_music[MUS_COUNT];
static SAMPLE *s_sfx[SFX_COUNT];
static SBYTE   music_voice = -1;
static int     cur_track   = -1;

/* ---- in-memory MREADER (lets MikMod parse a WAV from a RAM buffer) -------- */
typedef struct { MREADER core; const unsigned char *data; long size, pos; } MemReader;

static BOOL mr_eof(MREADER *r)  { MemReader *m = (MemReader *)r; return m->pos >= m->size; }
static long mr_tell(MREADER *r) { return ((MemReader *)r)->pos; }
static int  mr_get(MREADER *r)
{
	MemReader *m = (MemReader *)r;
	return (m->pos >= m->size) ? EOF : m->data[m->pos++];
}
static BOOL mr_read(MREADER *r, void *dst, size_t n)
{
	MemReader *m = (MemReader *)r;
	long rem = m->size - m->pos;
	if ((long)n > rem) { if (rem > 0) { memcpy(dst, m->data + m->pos, rem); m->pos += rem; } return 0; }
	memcpy(dst, m->data + m->pos, n); m->pos += n; return 1;
}
static BOOL mr_seek(MREADER *r, long off, int whence)
{
	MemReader *m = (MemReader *)r;
	long base = (whence == SEEK_SET) ? 0 : (whence == SEEK_CUR) ? m->pos : m->size;
	long np = base + off;
	if (np < 0 || np > m->size) return -1;
	m->pos = np; return 0;
}
static SAMPLE *load_wav(const void *data, long size)
{
	MemReader mr;
	mr.core.Seek = mr_seek; mr.core.Tell = mr_tell; mr.core.Read = mr_read;
	mr.core.Get = mr_get;   mr.core.Eof = mr_eof;
	mr.data = (const unsigned char *)data; mr.size = size; mr.pos = 0;
	return Sample_LoadGeneric(&mr.core);
}

/* ---- PCM synthesis ------------------------------------------------------- */
/* Append a square-wave segment sweeping f0->f1 over ms at amp; f0<=0 => silence.
 * Short attack/release ramps avoid clicks; phase carries across calls. */
static int seg(SWORD *buf, int cap, int pos, double f0, double f1,
               int ms, int amp, double *phase)
{
	int n = (int)((long)ms * SR / 1000); if (n < 1) n = 1;
	int atk = SR * 3 / 1000, rel = SR * 10 / 1000;
	for (int i = 0; i < n && pos < cap; i++, pos++) {
		if (f0 <= 0 && f1 <= 0) { buf[pos] = 0; continue; }
		double frac = (double)i / n;
		double f = f0 + (f1 - f0) * frac;
		*phase += f / SR;
		if (*phase >= 1.0) *phase -= (double)(long)*phase;
		double e = 1.0;
		if (i < atk)          e = (double)i / atk;
		else if (i > n - rel) e = (double)(n - i) / rel;
		double v = (*phase < 0.5) ? amp : -amp;
		int s = (int)(v * e);
		buf[pos] = (SWORD)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
	}
	return pos;
}

static void put_le32(unsigned char *p, u32 v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static void put_le16(unsigned char *p, u16 v) { p[0]=v; p[1]=v>>8; }

/* Wrap PCM (n mono 16-bit samples) in a little-endian WAV and load it. */
static SAMPLE *load_pcm(const SWORD *pcm, int n)
{
	long data = (long)n * 2, total = 44 + data;
	unsigned char *w = (unsigned char *)malloc(total);
	if (!w) return NULL;
	memcpy(w, "RIFF", 4);       put_le32(w + 4, (u32)(36 + data));
	memcpy(w + 8, "WAVE", 4);
	memcpy(w + 12, "fmt ", 4);  put_le32(w + 16, 16);
	put_le16(w + 20, 1);        put_le16(w + 22, 1);          /* PCM, mono   */
	put_le32(w + 24, SR);       put_le32(w + 28, SR * 2);     /* rate, byterate */
	put_le16(w + 32, 2);        put_le16(w + 34, 16);         /* block, bits */
	memcpy(w + 36, "data", 4);  put_le32(w + 40, (u32)data);
	for (int i = 0; i < n; i++) put_le16(w + 44 + i * 2, (u16)pcm[i]);
	SAMPLE *s = load_wav(w, total);
	free(w);
	return s;
}

#define SCRATCH (SR * 8)          /* up to 8 s per track */
static SWORD g_scratch[SCRATCH];

/* Build a single-voice square-wave track from parallel freq (Hz, 0=rest) and
 * duration (ms) arrays; `gap` ms of silence is carved out of each note's tail
 * for articulation. Returns an in-memory WAV SAMPLE (caller sets loop flags). */
static SAMPLE *synth_track(const int *mf, const int *ml, int count, int amp, int gap)
{
	double ph = 0; int n = 0;
	for (int k = 0; k < count; k++) {
		int dur = ml[k] - gap; if (dur < 1) dur = 1;
		n = seg(g_scratch, SCRATCH, n, mf[k], mf[k], dur, amp, &ph);
		if (gap) n = seg(g_scratch, SCRATCH, n, 0, 0, gap, 0, &ph);
	}
	return load_pcm(g_scratch, n);
}

/* Strategy / trading-card-game (Yu-Gi-Oh-ish) flavour: minor keys, arpeggiated
 * chord sweeps over the i-VI-III-VII progression, driving and a bit dramatic. */

/* title: epic A-minor arpeggio sweep (Am-F-C-G) rising to a held tonic (~4 s). */
static SAMPLE *synth_title(void) {
	static const int f[] = { 220,262,330,440, 175,220,262,349,   /* Am, F  */
	                         262,330,392,523, 196,247,294,392,   /* C,  G  */
	                         440, 0 };                            /* hold A4 */
	static const int l[] = { 190,190,190,190, 190,190,190,190,
	                         190,190,190,190, 190,190,190,190,
	                         760, 300 };
	return synth_track(f, l, 18, 5000, 16);
}
/* menu: driving Am-F-C-G arpeggio ostinato, duel-menu energy (~2.4 s loop). */
static SAMPLE *synth_menu(void) {
	static const int f[] = { 220,330,440,330, 175,262,349,262,   /* Am, F  */
	                         262,392,523,392, 196,294,392,294 };  /* C,  G  */
	static const int l[] = { 150,150,150,150, 150,150,150,150,
	                         150,150,150,150, 150,150,150,150 };
	return synth_track(f, l, 16, 4400, 14);
}
/* ingame: slow, tense A-minor arpeggios with breathing room (~5 s loop). */
static SAMPLE *synth_ingame(void) {
	static const int f[] = { 110,165,220,262, 175,220,262,0,     /* Am, F.  */
	                         131,196,262,330, 98,147,196,0 };     /* C,  G.  */
	static const int l[] = { 260,260,260,260, 260,260,260,300,
	                         260,260,260,260, 260,260,260,400 };
	return synth_track(f, l, 16, 3400, 16);
}
/* auction: fast rising E-minor arpeggio with a moving bass — climax tension (~2 s). */
static SAMPLE *synth_auction(void) {
	static const int f[] = { 165,330,494,330, 165,330,494,330,   /* Em x2 */
	                         175,349,523,349, 196,392,587,392 };  /* F, G  */
	static const int l[] = { 110,110,110,110, 110,110,110,110,
	                         110,110,110,110, 110,110,110,110 };
	return synth_track(f, l, 16, 4200, 12);
}
/* jail: ominous "trap card" descent + low pulse, still a touch comedic (~3 s). */
static SAMPLE *synth_jail(void) {
	static const int f[] = { 392,349,311,294,262, 131, 0, 131,98, 0 };
	static const int l[] = { 220,220,220,220,350, 250, 120, 250,300, 300 };
	return synth_track(f, l, 10, 4400, 16);
}
/* win: big triumphant victory fanfare + flourish (one-shot, ~3 s). */
static SAMPLE *synth_win(void) {
	static const int f[] = { 523,659,784,1047, 784,880,784,659,523, 0 };
	static const int l[] = { 200,200,200,400, 200,200,200,200,700, 300 };
	return synth_track(f, l, 10, 5600, 16);
}

/* ---- SFX ----------------------------------------------------------------- */
static SAMPLE *synth_roll(void) {  /* quick upward chirp */
	double ph = 0; int n = seg(g_scratch, SCRATCH, 0, 440, 990, 90, 8000, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_buy(void) {   /* two-note C5->E5 "ka-ching" */
	double ph = 0; int n = 0;
	n = seg(g_scratch, SCRATCH, n, 523, 523, 70, 8000, &ph);
	n = seg(g_scratch, SCRATCH, n, 659, 659, 130, 8000, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_cash(void) {  /* rising E5 G5 C6 sparkle */
	double ph = 0; int n = 0;
	n = seg(g_scratch, SCRATCH, n, 659, 659, 70, 7000, &ph);
	n = seg(g_scratch, SCRATCH, n, 784, 784, 70, 7000, &ph);
	n = seg(g_scratch, SCRATCH, n, 1047, 1047, 120, 7000, &ph);
	return load_pcm(g_scratch, n);
}
static SAMPLE *synth_error(void) { /* low descending buzz */
	double ph = 0; int n = 0;
	n = seg(g_scratch, SCRATCH, n, 220, 220, 90, 7000, &ph);
	n = seg(g_scratch, SCRATCH, n, 175, 175, 140, 7000, &ph);
	return load_pcm(g_scratch, n);
}

/* ---- public API ---------------------------------------------------------- */
void audio_init(void)
{
	if (audio_ok) return;

	MikMod_RegisterAllDrivers();
	MikMod_RegisterAllLoaders();

	md_mode = DMODE_STEREO | DMODE_16BITS | DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
	md_mixfreq = 48000;

	if (MikMod_Init("")) return;                       /* silent on failure */
	MikMod_SetNumVoices(0, 8);
	if (MikMod_EnableOutput()) { MikMod_Exit(); return; }

	s_music[MUS_TITLE]   = synth_title();
	s_music[MUS_MENU]    = synth_menu();
	s_music[MUS_INGAME]  = synth_ingame();
	s_music[MUS_AUCTION] = synth_auction();
	s_music[MUS_JAIL]    = synth_jail();
	s_music[MUS_WIN]     = synth_win();
	/* loop every track except the WIN fanfare (one-shot on game over) */
	for (int t = 0; t < MUS_COUNT; t++) {
		if (!s_music[t] || t == MUS_WIN) continue;
		s_music[t]->flags    |= SF_LOOP;
		s_music[t]->loopstart = 0;
		s_music[t]->loopend   = s_music[t]->length;
	}

	s_sfx[SFX_ROLL]  = synth_roll();
	s_sfx[SFX_BUY]   = synth_buy();
	s_sfx[SFX_CASH]  = synth_cash();
	s_sfx[SFX_ERROR] = synth_error();

	audio_ok = 1;   /* audio_music() is driven by the game loop from here */
}

void audio_update(void)   { if (audio_ok) MikMod_Update(); }

void audio_shutdown(void)
{
	if (!audio_ok) return;
	audio_ok = 0;
	if (music_voice >= 0) Voice_Stop(music_voice);
	for (int t = 0; t < MUS_COUNT; t++) if (s_music[t]) Sample_Free(s_music[t]);
	for (int i = 0; i < SFX_COUNT; i++) if (s_sfx[i])   Sample_Free(s_sfx[i]);
	MikMod_DisableOutput();
	MikMod_Exit();
}

void audio_music(int track)
{
	if (!audio_ok || track < 0 || track >= MUS_COUNT) return;
	if (track == cur_track || !s_music[track]) return;
	cur_track = track;
	if (music_voice >= 0) Voice_Stop(music_voice);
	music_voice = Sample_Play(s_music[track], 0, SFX_CRITICAL);   /* won't be stolen */
	if (music_voice >= 0) Voice_SetVolume(music_voice, 96);       /* duck under SFX  */
}

void audio_sfx(int id)
{
	if (audio_ok && id >= 0 && id < SFX_COUNT && s_sfx[id])
		Sample_Play(s_sfx[id], 0, 0);
}

void audio_play_blip(void) { audio_sfx(SFX_ROLL); }
