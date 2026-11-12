/* Cobalt Looper core: per-track Cue arm + Cue Mute transport, bar-quantized,
 * internal or external (MIDI 24 ppqn) clock.  Freestanding C11: no malloc, no libc
 * beyond string.h.  Audio side (lp_process) is meant for the M7 audio core; the
 * control side (lp_cmd) is safe to call from the UI core or an IRQ (single producer). */
#ifndef COBALT_LOOPER_H
#define COBALT_LOOPER_H
#include <stdint.h>
#include <stddef.h>

#ifndef LP_MAX_TRACKS
#define LP_MAX_TRACKS 8          /* set to the bluebox's real track count */
#endif
#ifndef LP_MAX_BARS
#define LP_MAX_BARS 8
#endif
#ifndef LP_BARRIER
#define LP_BARRIER() __sync_synchronize()   /* use __DMB() on the M7/M4 */
#endif
#define LP_BEATS_PER_BAR 4
#define LP_PPQN 24
#define LP_QN 32                 /* command queue size, power of two */

typedef enum {
  LP_CMD_ARM_TOGGLE,  /* a = track                         (Tracks page: Cue button)   */
  LP_CMD_TAP,         /* Cue Mute tap: rec -> play -> dub  (Main page)                 */
  LP_CMD_HOLD,        /* Cue Mute hold: stop + clear                                   */
  LP_CMD_WET,         /* f = 0..1 dry/wet                  (Main page: Cue volume)     */
  LP_CMD_BPM,         /* f = bpm, internal clock only, ignored while a loop exists     */
  LP_CMD_LEAD,        /* a = 0 next bar line, 1 = one extra bar                        */
  LP_CMD_SRC,         /* a = 0 internal, 1 external                                    */
  LP_CMD_TICK,        /* t = lp_now() value when the MIDI 0xF8 arrived                 */
  LP_CMD_START, LP_CMD_STOP, LP_CMD_CONT   /* MIDI 0xFA / 0xFC / 0xFB                 */
} lp_cmd_type;

typedef struct { uint8_t type, a; float f; int64_t t; } lp_cmd_t;
typedef struct { float *buf[2]; size_t len; uint8_t armed, has; } lp_track_t;

typedef struct {
  float sr; double bpm, bar_len, E; float wet;
  lp_track_t tr[LP_MAX_TRACKS];
  int64_t clk, abs, start_at, stop_at, last_t, last_tick;
  size_t wp, pos, loop_len, max_len, cap;
  uint32_t n; uint8_t lead, ext, stopped, clk_ok, run, has_last, recording, mode, playing;
  volatile uint32_t qh, qt; lp_cmd_t q[LP_QN];
} lp_t;

void    lp_init(lp_t *p, float sample_rate);
void    lp_attach(lp_t *p, int track, float *l, float *r, size_t frames); /* caller-owned RAM */
int     lp_cmd(lp_t *p, lp_cmd_t c);                                      /* 0 = queue full  */
void    lp_process(lp_t *p, float *const io[][2], int ntracks, int frames); /* in place      */
int64_t lp_now(const lp_t *p);            /* free-running frame counter, for timestamping ticks */
int     lp_light(const lp_t *p);          /* Cue Mute LED: 0 off, 1 on (recording), 2 blink (waiting for bar) */
int     lp_bars(const lp_t *p);           /* loop length in bars, 0 if none */
#endif
