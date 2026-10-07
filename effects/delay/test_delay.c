/*
 * test_delay.c - desktop tests for the delay DSP core.
 *
 * Runs a few checks on delay.c, then renders a short demo WAV you can
 * listen to. Build and run with effects/delay/run_tests.sh.
 */
#include "delay.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FS 48000.0f
#define N  (48000 * 3)

static Delay g_delay;          /* 64 KB - too big for the stack on some setups */
static float g_in[N], g_out[N];
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

static void clear_input(void)
{
    for (int i = 0; i < N; i++)
        g_in[i] = 0.0f;
}

/* Index of the largest |out| in [from, to). */
static int peak_index(int from, int to)
{
    int best = from;
    for (int i = from; i < to; i++)
        if (fabsf(g_out[i]) > fabsf(g_out[best]))
            best = i;
    return best;
}

static void test_impulse(void)
{
    printf("Impulse response (300 ms, feedback 0.5, level 1.0)\n");
    Delay *d = &g_delay;
    delay_init(d, FS);
    delay_set_time_ms(d, 300.0f);
    d->cur_samples = d->target_samples;
    delay_set_feedback(d, 0.5f);
    delay_set_level(d, 1.0f);
    delay_set_tone(d, 1.0f);

    clear_input();
    g_in[0] = 0.25f;
    /* Process in 32-sample blocks, like the DMA callback will. */
    for (int i = 0; i < N; i += 32)
        delay_process(d, &g_in[i], &g_out[i], 32);

    const int T = (int)(0.3f * FS);

    float max_gap = 0.0f;
    for (int i = 1; i < T - 2; i++)
        max_gap = fmaxf(max_gap, fabsf(g_out[i]));
    CHECK(max_gap < 0.01f, "silence before first echo (max %.4f)", max_gap);

    int p1 = peak_index(T - 50, T + 50);
    CHECK(abs(p1 - T) <= 1, "first echo at sample %d (expected %d)", p1, T);
    CHECK(fabsf(g_out[p1] - 0.245f) < 0.01f,
          "first echo amplitude %.4f (expected ~0.245)", g_out[p1]);

    int p2 = peak_index(2 * T - 50, 2 * T + 50);
    CHECK(fabsf(g_out[p2]) < fabsf(g_out[p1]) && fabsf(g_out[p2]) > 0.02f,
          "second echo quieter: %.4f at sample %d", g_out[p2], p2);
}

/* Feed an impulse, optionally bypass after 1000 samples, return the peak of
 * the third echo. */
static float third_echo(bool bypass)
{
    Delay *d = &g_delay;
    delay_init(d, FS);
    delay_set_time_ms(d, 100.0f);
    d->cur_samples = d->target_samples;
    delay_set_feedback(d, 0.6f);
    delay_set_level(d, 1.0f);

    clear_input();
    g_in[0] = 0.5f;
    delay_process(d, g_in, g_out, 1000);   /* engaged: note goes in */
    delay_set_enabled(d, !bypass);
    delay_process(d, &g_in[1000], &g_out[1000], 20000);

    int T = (int)(0.1f * FS);
    return fabsf(g_out[peak_index(3 * T - 50, 3 * T + 50)]);
}

static void test_tails(void)
{
    printf("Bypass keeps repeats ringing (tails)\n");
    float engaged = third_echo(false);
    float bypassed = third_echo(true);
    CHECK(bypassed > 0.0f && fabsf(bypassed - engaged) < 1e-6f,
          "third echo after bypass %.4f matches engaged %.4f", bypassed, engaged);
}

static void test_stability(void)
{
    printf("Stability at max feedback with loud noise + knob sweeps\n");
    Delay *d = &g_delay;
    delay_init(d, FS);
    delay_set_feedback(d, 1.0f);
    delay_set_level(d, 1.0f);
    delay_set_tone(d, 1.0f);

    srand(1);
    for (int i = 0; i < N; i++)
        g_in[i] = 2.0f * (float)rand() / (float)RAND_MAX - 1.0f;

    bool finite = true;
    float max_out = 0.0f;
    for (int i = 0; i < N; i += 32) {
        /* Sweep delay time up and down while running. */
        delay_set_time_ms(d, 20.0f + 600.0f * (float)((i / 4800) % 2));
        delay_process(d, &g_in[i], &g_out[i], 32);
        for (int j = i; j < i + 32; j++) {
            finite = finite && isfinite(g_out[j]);
            max_out = fmaxf(max_out, fabsf(g_out[j]));
        }
    }
    CHECK(finite, "no NaN/Inf in output");
    CHECK(max_out <= 2.1f, "output bounded (max %.3f)", max_out);
}

static void put_u16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc(v >> 8, f); }
static void put_u32(FILE *f, uint32_t v) { put_u16(f, v & 0xffff); put_u16(f, v >> 16); }

static int write_wav(const char *path, const float *x, int n, int fs)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    fwrite("RIFF", 1, 4, f); put_u32(f, 36 + n * 2);
    fwrite("WAVEfmt ", 1, 8, f);
    put_u32(f, 16); put_u16(f, 1); put_u16(f, 1);  /* PCM, mono */
    put_u32(f, fs); put_u32(f, fs * 2); put_u16(f, 2); put_u16(f, 16);
    fwrite("data", 1, 4, f); put_u32(f, n * 2);
    for (int i = 0; i < n; i++) {
        float s = fmaxf(-1.0f, fminf(1.0f, x[i]));
        put_u16(f, (uint16_t)(int16_t)(s * 32767.0f));
    }
    fclose(f);
    return 0;
}

static void render_demo(const char *path)
{
    /* Two plucked notes through a 350 ms delay. */
    Delay *d = &g_delay;
    delay_init(d, FS);
    delay_set_time_ms(d, 350.0f);
    d->cur_samples = d->target_samples;
    delay_set_feedback(d, 0.5f);
    delay_set_level(d, 0.6f);
    delay_set_tone(d, 0.4f);

    clear_input();
    const float notes[2][2] = { { 0.0f, 220.0f }, { 1.2f, 329.6f } };
    for (int k = 0; k < 2; k++) {
        int start = (int)(notes[k][0] * FS);
        for (int i = 0; start + i < N && i < (int)(0.6f * FS); i++) {
            float t = (float)i / FS;
            g_in[start + i] += 0.4f * expf(-6.0f * t) *
                               sinf(6.28318530718f * notes[k][1] * t);
        }
    }

    delay_process(d, g_in, g_out, N);
    if (write_wav(path, g_out, N, (int)FS) == 0)
        printf("Wrote demo audio to %s\n", path);
    else
        printf("Could not write %s\n", path);
}

int main(int argc, char **argv)
{
    test_impulse();
    test_tails();
    test_stability();

    render_demo(argc > 1 ? argv[1] : "delay_demo.wav");

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
