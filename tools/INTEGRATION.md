# Cobalt Looper: firmware integration

`cobalt_looper.c/.h` is the whole feature as a freestanding C11 module (no malloc, float DSP).
`test_looper.c` runs it on a host: `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined cobalt_looper.c test_looper.c -lm && ./a.out`.
It checks bar-quantized start/stop, whole-bar loop length at 60/97.3/120 BPM, glitch-free wrap-around
playback, overdub, hold-to-clear, unarmed tracks untouched, and the external clock (tempo lock,
Start/Stop/Continue, clock loss).

## What still has to be done against the real firmware (needs BLUEBOX.BIN 1.5.2 + the repo's src/)
Same job the existing Cobalt modes did: find each address in the 1.5.2 image and add a hook.

| # | Hook point to find | What the hook calls |
|---|--------------------|---------------------|
| 1 | Tracks page, Cue button press handler (UI core) | `lp_cmd(ARM_TOGGLE, a=track)`; keep the stock Cue action only if you still want it |
| 2 | Main page, Cue Mute button press / release | short press: `LP_CMD_TAP`; held ~1 s: `LP_CMD_HOLD` |
| 3 | Main page, Cue LED draw | colour from `lp_light()` (0 off, 1 solid, 2 blink) |
| 4 | Main page, Cue volume knob/fader | `LP_CMD_WET` = value 0..1 (dry = incoming, wet = loop) |
| 5 | Audio core, per-track block, after the input/gate stage and before the track goes to the Cue/main sum | `lp_process(&lp, io, ntracks, frames)` once per block, all tracks together |
| 6 | Tempo source: the project BPM variable / tempo clock | internal: `LP_CMD_BPM` on change |
| 7 | MIDI realtime handler (0xF8 / 0xFA / 0xFB / 0xFC) | `LP_CMD_TICK` with `t = lp_now(&lp)`, `LP_CMD_START/CONT/STOP`; send `LP_CMD_SRC` when the clock-source setting changes |

Open items that depend on the hardware:
* **Memory.** `lp_attach()` takes caller-owned buffers. 8 bars at 120 BPM / 48 kHz stereo float is about 6 MB per track. Check free SDRAM, then lower `LP_MAX_BARS`, limit the number of armed tracks, or switch the buffers to int16.
* **Core split.** If the UI runs on the M4 and audio on the M7, `lp_t` lives in shared memory the M7 can see; the queue is single-producer, so give each producer (UI, MIDI IRQ) its own `lp_t` queue or serialise them. Use `__DMB()` for `LP_BARRIER`.
* **Tick timing.** Ticks are timestamped in audio frames. For tighter lock, capture the timestamp in the MIDI IRQ rather than at block start.
* **Float.** Assumes the audio core has a single-precision FPU. Control-path math uses double; the per-sample path does not.
* **Sample rate and track count.** Set `LP_MAX_TRACKS` and the `lp_init()` rate to the real values.
* **Build.** Payloads in this repo are built with Arm GNU Toolchain 13.2.Rel1; this module has only been compiled with host gcc here.
* **Settings.** Cue arm state and wet level aren't persisted; add them to the project save if you want that.

## Behaviour differences from the web prototype
* With the external clock stopped or lost, tracks pass through untouched (the prototype went silent).
* Clearing doesn't wipe memory: `has` flags gate every read and a new take overwrites the whole region.
* The tempo command is ignored while a loop, recording or pending start exists.
