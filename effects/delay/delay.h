/*
 * delay.h - digital delay DSP core.
 *
 * Pure C with no HAL dependencies, so it builds both for the STM32 and on a
 * desktop machine for testing (see run_tests.sh).
 *
 * Threading model: the delay_set_*() functions are called from the main loop,
 * delay_process() is called from the audio DMA callback. Parameters are single
 * 32-bit floats/bools, which are written atomically on Cortex-M4.
 */
#ifndef DELAY_H
#define DELAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Delay line length in samples. Must be a power of two.
 * 32768 x int16 = 64 KB of RAM -> ~680 ms at 48 kHz. */
#define DELAY_BUF_LEN  32768u
#define DELAY_BUF_MASK (DELAY_BUF_LEN - 1u)

typedef struct {
    int16_t buf[DELAY_BUF_LEN];
    uint32_t write_idx;
    float sample_rate;

    /* Parameters (written by main loop) */
    volatile float target_samples; /* requested delay time in samples */
    volatile float feedback;       /* 0..1 */
    volatile float level;          /* wet level, 0..1 */
    volatile float tone_coef;      /* one-pole LPF coefficient for repeats */
    volatile bool enabled;         /* false = bypass, repeats ring out (tails) */

    /* DSP state (audio callback only) */
    float cur_samples;             /* smoothed delay time */
    float glide_coef;
    float tone_state;
    float dc_x1, dc_y1;
} Delay;

void  delay_init(Delay *d, float sample_rate);
void  delay_set_time_ms(Delay *d, float ms);
void  delay_set_feedback(Delay *d, float feedback);
void  delay_set_level(Delay *d, float level);
void  delay_set_tone(Delay *d, float tone);     /* 0 = dark, 1 = bright */
void  delay_set_enabled(Delay *d, bool enabled);
float delay_max_time_ms(const Delay *d);

/* Process n samples. Audio is float in [-1, 1]; in and out may alias. */
void  delay_process(Delay *d, const float *in, float *out, size_t n);

#endif /* DELAY_H */
