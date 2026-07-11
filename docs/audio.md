# Music & SFX — spec (synthesized, implemented later)

Audio stays **code-synthesized** (no asset files): square-wave PCM built in
`source/audio.c` and played through MikMod, exactly like the current blip + loop. This
doc describes each track as a note sequence so it can be implemented later by extending
the existing pattern.

## How tracks are built (current engine)

`source/audio.c` already has the machinery:
- `seg(buf, cap, &pos, f0, f1, ms, amp, &phase)` appends a **square-wave** segment
  (sweeps `f0→f1` over `ms`, 3 ms attack / 10 ms release to avoid clicks), phase carried.
- `synth_music()` builds a track from two arrays — `mf[]` note frequencies (Hz, `0` =
  rest) and `ml[]` durations (ms) — then `load_pcm()` wraps it as an in-memory WAV and
  `Sample_LoadGeneric` loads it; the music sample is flagged `SF_LOOP` and played
  `SFX_CRITICAL` at reduced volume so SFX can duck over it.

To **add a track**: write a `synth_<name>()` that fills `mf[]/ml[]`, keep a `SAMPLE*`
for it, and swap the looping voice with `Voice_Stop(music_voice)` +
`Sample_Play(new, 0, SFX_CRITICAL)` when the screen changes. 8 voices are reserved, so a
simple **2-voice harmony** (melody + a bass/harmony line on a second voice) is possible.

**Note→frequency**: A4 = 440 Hz, each semitone ×2^(1/12). Handy: C4 262, D4 294, E4 330,
F4 349, G4 392, A4 440, B4 494, C5 523, E5 659, G5 784. `0` = rest.

⚠️ Hand-built WAV bytes must be **little-endian** (the PPU is big-endian) — use the
existing `put_le16/32` helpers.

## Tracks (per screen)

Notation below is melody in note names with durations; keep everything square-wave,
bright-but-not-harsh, ~2–8 s loops. Tempos are a guide.

| track | screen | key / tempo | mood | melody sketch (loop) |
|---|---|---|---|---|
| **title** | title | C major, 100 BPM | grand, inviting | rising fanfare **C4 E4 G4 C5** (¼ notes) → held **G4** (½) → **E4 C4** ; let it ring, ~4 s, loop |
| **menu** | main menu / setup / how-to | C major, 110 BPM | light, upbeat, loopable | bouncy **E4 G4 E4 C4 · D4 F4 D4 B3** two bars, then **G4 E4 C4** turnaround; 8 s loop, low volume |
| **ingame** | during a match | A minor, 96 BPM | calm, unobtrusive (plays for a long time) | sparse walking bass **A2 E3 A2 C3** with occasional melody stabs **A4 · C5 · B4 · G4**; keep it airy, ~12 s loop |
| **auction** | auction overlay | E minor, 140 BPM | tense, driving | fast repeating **E4 E4 F4 E4 D4** ostinato with a climbing bass **E2→B2**; short 3 s loop, adds urgency |
| **jail** | jail overlay | C minor, 90 BPM | comedic, "uh-oh" | descending **G4 F4 Eb4 D4 C4** with a plodding **C2 C2 G2** bass; light, playful, 4 s loop |
| **win** | game-over screen | C major, 120 BPM | triumphant fanfare (one-shot) | **C5 C5 C5 · E5 · G5** held, then **G5 F5 E5 F5 G5** flourish; NOT looped, play once |
| **bankrupt** | when a player is eliminated | A minor, 80 BPM | sombre sting (one-shot) | slow **A4 G4 F4 E4** then low **A2**; short, one-shot |
| **trade** | trade builder / offer | F major, 108 BPM | neutral, "negotiation" | gentle **F4 A4 C5 A4** arpeggio, repeats; soft, 4 s loop (optional — can reuse `menu`) |

## SFX (extend `synth_blip`)

Short one-shots, `Sample_Play(..., 0, 0)`, ~60–120 ms:

| sfx | trigger | sketch |
|---|---|---|
| **roll** | rolling dice | quick noisy rattle → a bright **up-chirp 440→990** (the current blip works) |
| **buy** | buying a property | two-note **C5 → E5** ka-ching |
| **build** | build house/hotel | short **G4** tap, a touch of noise (a "clack") |
| **cash** | collecting money (GO, rent in) | rising **E5 G5 C6** sparkle |
| **error** | rejected / can't afford | low **A3 → F3** buzz (short square, no sweep) |
| **select / move** | menu focus move | very short **E5** tick |

Keep SFX voices reserved (`MikMod_SetNumVoices(music, sfx)`) and duck the music voice
(`Voice_SetVolume`) so effects cut through.

## Init note

A bad MikMod init can **hang the console** (double-init is a known freeze) — keep the
defensive `audio_ok` guard already in `audio.c`: on any failure, all audio calls become
silent no-ops.
