/*
 * delay.c - digital delay DSP core. See delay.h.
 */
#include "delay.h"

#include <math.h>
#include <string.h>

#define TWO_PI           6.28318530718f
#define DEFAULT_TIME_MS  300.0f
#define GLIDE_TIME_S     0.08f   /* how fast delay time follows the knob */
#define DC_BLOCK_R       0.995f  /* DC blocker pole, ~38 Hz at 48 kHz */
#define TONE_MIN_HZ      1000.0f
#define TONE_MAX_HZ      10000.0f

static float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* Pade approximation of tanh: unity gain for small signals, saturates
 * smoothly to +/-1. Keeps high feedback settings from blowing up. */
static float soft_clip(float x)
{
    x = clampf(x, -3.0f, 3.0f);
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* Read the line `delay` samples behind the write head, with linear
 * interpolation so the delay time can change smoothly. */
static float read_frac(const Delay *d, float delay)
{
    float pos = (float)d->write_idx - delay;
    if (pos < 0.0f)
        pos += (float)DELAY_BUF_LEN;

    uint32_t i0 = (uint32_t)pos;
    float frac = pos - (float)i0;
    uint32_t i1 = (i0 + 1u) & DELAY_BUF_MASK;
    i0 &= DELAY_BUF_MASK;

    float a = (float)d->buf[i0];
    float b = (float)d->buf[i1];
    return (a + frac * (b - a)) * (1.0f / 32767.0f);
}

void delay_init(Delay *d, float sample_rate)
{
    memset(d, 0, sizeof(*d));
    d->sample_rate = sample_rate;
    d->glide_coef = 1.0f - expf(-1.0f / (sample_rate * GLIDE_TIME_S));
    d->enabled = true;

    delay_set_time_ms(d, DEFAULT_TIME_MS);
    d->cur_samples = d->target_samples;   /* no glide on startup */
    delay_set_feedback(d, 0.4f);
    delay_set_level(d, 0.5f);
    delay_set_tone(d, 0.6f);
}

void delay_set_time_ms(Delay *d, float ms)
{
    float samples = ms * 0.001f * d->sample_rate;
    d->target_samples = clampf(samples, 1.0f, (float)(DELAY_BUF_LEN - 2u));
}

void delay_set_feedback(Delay *d, float feedback)
{
    /* 1.0 = endless repeats / self-oscillation, limited by soft_clip(). */
    d->feedback = clampf(feedback, 0.0f, 1.0f);
}

void delay_set_level(Delay *d, float level)
{
    d->level = clampf(level, 0.0f, 1.0f);
}

void delay_set_tone(Delay *d, float tone)
{
    tone = clampf(tone, 0.0f, 1.0f);
    /* Exponential knob taper between TONE_MIN_HZ and TONE_MAX_HZ. */
    float fc = TONE_MIN_HZ * powf(TONE_MAX_HZ / TONE_MIN_HZ, tone);
    fc = fminf(fc, 0.45f * d->sample_rate);
    d->tone_coef = 1.0f - expf(-TWO_PI * fc / d->sample_rate);
}

void delay_set_enabled(Delay *d, bool enabled)
{
    d->enabled = enabled;
}

float delay_max_time_ms(const Delay *d)
{
    return (float)(DELAY_BUF_LEN - 2u) * 1000.0f / d->sample_rate;
}

void delay_process(Delay *d, const float *in, float *out, size_t n)
{
    /* Snapshot parameters once per block. */
    const float target = d->target_samples;
    const float fb = d->feedback;
    const float level = d->level;
    const float tone = d->tone_coef;
    const bool on = d->enabled;

    for (size_t i = 0; i < n; i++) {
        /* Remove ADC bias offset. */
        float x = in[i];
        float y = x - d->dc_x1 + DC_BLOCK_R * d->dc_y1;
        d->dc_x1 = x;
        d->dc_y1 = y;
        x = y;

        /* Glide toward the requested time (tape-style pitch bend). */
        d->cur_samples += d->glide_coef * (target - d->cur_samples);

        float wet = read_frac(d, d->cur_samples);

        /* Low-pass the feedback path so each repeat gets darker. */
        d->tone_state += tone * (wet - d->tone_state);

        /* When bypassed, stop feeding new input but let repeats ring out. */
        float write = (on ? x : 0.0f) + fb * d->tone_state;
        d->buf[d->write_idx] = (int16_t)(soft_clip(write) * 32767.0f);
        d->write_idx = (d->write_idx + 1u) & DELAY_BUF_MASK;

        /* Dry stays at unity so bypass doesn't change volume. */
        out[i] = x + level * wet;
    }
}
