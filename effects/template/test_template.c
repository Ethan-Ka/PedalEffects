/*
 * test_template.c - desktop tests for the template effect.
 *
 * Checks the input -> effect -> output path is transparent. Once you add
 * processing, replace these with checks for your effect (see
 * effects/delay/test_delay.c). Build and run with
 * effects/template/run_tests.sh.
 */
#include "template.h"
#include "audio_io.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define FS 48000.0f

static Template g_fx;
static int g_failures;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (cond) {                                         \
            printf("  PASS  ");                             \
        } else {                                            \
            printf("  FAIL  ");                             \
            g_failures++;                                   \
        }                                                   \
        printf(__VA_ARGS__);                                \
        printf("\n");                                       \
    } while (0)

static void process(void *ctx, const float *in, float *out, size_t n)
{
    template_process(ctx, in, out, n);
}

static void test_float_passthrough(void)
{
    printf("Float passthrough\n");
    template_init(&g_fx, FS);

    float in[1000], out[1000];
    for (int i = 0; i < 1000; i++)
        in[i] = sinf(0.01f * (float)i);
    template_process(&g_fx, in, out, 1000);

    float max_err = 0.0f;
    for (int i = 0; i < 1000; i++)
        max_err = fmaxf(max_err, fabsf(out[i] - in[i]));
    CHECK(max_err == 0.0f, "output equals input (max error %g)", max_err);
}

static void test_adc_dac_roundtrip(void)
{
    printf("ADC -> effect -> DAC, every 12-bit code\n");
    template_init(&g_fx, FS);

    static uint16_t adc[4096], dac[4096];
    for (int i = 0; i < 4096; i++)
        adc[i] = (uint16_t)i;
    audio_process_adc_dac(adc, dac, 4096, process, &g_fx);

    int mismatches = 0;
    for (int i = 0; i < 4096; i++)
        mismatches += dac[i] != adc[i];
    CHECK(mismatches == 0, "DAC code equals ADC code (%d mismatches)", mismatches);
}

int main(void)
{
    test_float_passthrough();
    test_adc_dac_roundtrip();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
