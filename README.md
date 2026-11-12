# Cobalt Looper

A looper for the 1010music bluebox desktop, designed as a mod for
[bluebox-cobalt-mod](https://github.com/sbsmnt/bluebox-cobalt-mod) (firmware 1.5.2).

* **Tracks page:** the Cue button on each track arms it for looping.
* **Main page:** the Cue Mute button is the transport. Tap to record (LED solid, blinking while it waits
  for the bar), tap again to stop and play the loop, tap again to overdub, hold to stop and clear.
* **Main page:** the Cue volume is dry/wet between the incoming audio and the loop. Armed tracks go through
  the Cue insert to the main output.
* Starts and stops land on bar lines of the master clock, which can be internal or external MIDI clock.

## Layout
| Path | What |
|------|------|
| `src/` | Freestanding C11 looper core (`cobalt_looper.c/.h`) |
| `test/` | Host-side tests: `make test` |
| `prototype/index.html` | Browser mock-up of the behaviour (open in a browser, press Start audio) |
| `docs/INTEGRATION.md` | Hook points and open items for the real firmware |

## Status
The core is tested on a PC only. It is **not yet hooked into the firmware** and has not been built with the
Arm toolchain. See `docs/INTEGRATION.md`. No firmware is included; patching needs your own copy of the
official image, as with Cobalt itself.
