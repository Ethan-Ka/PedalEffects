/*
 * live_delay.c - play the delay live through the Mac's speakers.
 *
 * Press n to pluck a note, Ctrl-C to quit.
 */

/* ---- Knobs: edit these and rerun effects/delay/run_tests.sh ---- */
#define KNOB_TIME_MS   350.0f   /* delay time, 1 .. ~680 ms */
#define KNOB_FEEDBACK  0.5f     /* 0 = one repeat, 1 = endless */
#define KNOB_LEVEL     0.6f     /* volume of the repeats, 0 .. 1 */
#define KNOB_TONE      0.5f     /* 0 = dark repeats, 1 = bright */

#include "delay.h"
#include "live_host.h"

#include <stdio.h>

#define FS 48000.0f

static Delay g_delay;

static void process(void *ctx, const float *in, float *out, size_t n)
{
    delay_process(ctx, in, out, n);
}

int main(void)
{
    delay_init(&g_delay, FS);
    delay_set_time_ms(&g_delay, KNOB_TIME_MS);
    g_delay.cur_samples = g_delay.target_samples;
    delay_set_feedback(&g_delay, KNOB_FEEDBACK);
    delay_set_level(&g_delay, KNOB_LEVEL);
    delay_set_tone(&g_delay, KNOB_TONE);

    printf("Delay live: time %.0f ms, feedback %.2f, level %.2f, tone %.2f\n",
           KNOB_TIME_MS, KNOB_FEEDBACK, KNOB_LEVEL, KNOB_TONE);
    if (KNOB_TIME_MS > delay_max_time_ms(&g_delay))
        printf("(time capped at %.0f ms by the buffer size)\n",
               delay_max_time_ms(&g_delay));

    return live_host_run(FS, process, &g_delay);
}
