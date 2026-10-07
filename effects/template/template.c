/*
 * template.c - passthrough effect skeleton. See template.h.
 */
#include "template.h"

#include <string.h>

void template_init(Template *t, float sample_rate)
{
    memset(t, 0, sizeof(*t));
    t->sample_rate = sample_rate;

    /* Set default knob values here. */
}

void template_process(Template *t, const float *in, float *out, size_t n)
{
    /* Snapshot knob values once per block here, e.g.
     * const float gain = t->gain; */
    (void)t;

    for (size_t i = 0; i < n; i++) {
        float x = in[i];        /* input sample */

        /* ---- effect processing goes here ---- */
        float y = x;

        out[i] = y;             /* output sample */
    }
}
