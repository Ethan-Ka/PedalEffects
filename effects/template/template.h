/*
 * template.h - starting point for a new effect.
 *
 * To make a new effect:
 *   1. Copy effects/template/ to effects/<name>/
 *   2. Rename the files and replace template/Template/TEMPLATE with <name>
 *   3. Add knobs to the struct + setters, and your DSP in template_process()
 *
 * Pure C with no HAL dependencies, so it builds both for the STM32 and on a
 * desktop machine for testing (see run_tests.sh).
 *
 * Threading model: setters are called from the main loop, template_process()
 * from the audio DMA callback. Keep each parameter a single float/bool so
 * writes are atomic on Cortex-M4.
 */
#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <stddef.h>

typedef struct {
    float sample_rate;

    /* Parameters (written by main loop), e.g.
     * volatile float gain; */

    /* DSP state (audio callback only), e.g. filter memory */
} Template;

void template_init(Template *t, float sample_rate);

/* Add one setter per knob, e.g.
 * void template_set_gain(Template *t, float gain); */

/* Process n samples. Audio is float in [-1, 1]; in and out may alias. */
void template_process(Template *t, const float *in, float *out, size_t n);

#endif /* TEMPLATE_H */
