/*
 * audio_io.h - shared input/output plumbing for every effect.
 *
 * Effects work on blocks of float audio in [-1, 1]. This file converts
 * between that and the STM32's 12-bit ADC/DAC samples (mid-rail = 2048).
 * Header-only and HAL-free, so it builds on the STM32 and on a desktop.
 */
#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <stddef.h>
#include <stdint.h>

#define AUDIO_MAX_BLOCK 256

/* Signature every effect is called through. ctx points at the effect state;
 * in and out may be the same buffer. */
typedef void (*effect_process_fn)(void *ctx, const float *in, float *out, size_t n);

static inline float adc12_to_float(uint16_t v)
{
    return ((float)v - 2048.0f) * (1.0f / 2048.0f);
}

static inline uint16_t float_to_dac12(float x)
{
    float v = x * 2048.0f + 2048.5f;
    if (v < 0.0f)    v = 0.0f;
    if (v > 4095.0f) v = 4095.0f;
    return (uint16_t)v;
}

/* Input -> effect -> output for one block of ADC/DAC DMA samples.
 * Call from the DMA half/full-complete callbacks. */
static inline void audio_process_adc_dac(const uint16_t *adc, uint16_t *dac, size_t n,
                                         effect_process_fn process, void *ctx)
{
    float buf[AUDIO_MAX_BLOCK];

    while (n > 0) {
        size_t chunk = n < AUDIO_MAX_BLOCK ? n : AUDIO_MAX_BLOCK;

        for (size_t i = 0; i < chunk; i++)
            buf[i] = adc12_to_float(adc[i]);

        process(ctx, buf, buf, chunk);

        for (size_t i = 0; i < chunk; i++)
            dac[i] = float_to_dac12(buf[i]);

        adc += chunk;
        dac += chunk;
        n -= chunk;
    }
}

#endif /* AUDIO_IO_H */
