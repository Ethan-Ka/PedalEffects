/*
 * live_template.c - play the template effect live through the Mac's speakers.
 *
 * Press n to pluck a note, Ctrl-C to quit. With no processing yet, you hear
 * the plain pluck.
 */

/* ---- Knobs: edit these and rerun effects/template/run_tests.sh ----
 * e.g. #define KNOB_GAIN 0.5f */

#include "template.h"
#include "live_host.h"

#include <stdio.h>

#define FS 48000.0f

static Template g_fx;

static void process(void *ctx, const float *in, float *out, size_t n)
{
    template_process(ctx, in, out, n);
}

int main(void)
{
    template_init(&g_fx, FS);
    /* Apply knobs here, e.g. template_set_gain(&g_fx, KNOB_GAIN); */

    printf("Template live (passthrough)\n");
    return live_host_run(FS, process, &g_fx);
}
