/*
 * live_host.c - see live_host.h.
 *
 * Audio runs on a CoreAudio AudioQueue thread and calls the effect in
 * blocks, the same way the STM32 DMA callback will.
 */
#include "live_host.h"

#include <AudioToolbox/AudioToolbox.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define BLOCK       AUDIO_MAX_BLOCK  /* frames per audio buffer (~5 ms) */
#define NUM_BUFFERS 3
#define KS_MAX      1024
#define OUT_GAIN    0.6f             /* headroom: effects can exceed 1.0 */

/* A minor pentatonic, cycled on each press. */
static const float NOTES[] = { 220.00f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f };
#define NUM_NOTES ((int)(sizeof(NOTES) / sizeof(NOTES[0])))

static float g_fs;
static effect_process_fn g_process;
static void *g_ctx;
static atomic_int g_note_count;            /* written by main, read by audio */
static volatile sig_atomic_t g_running = 1;

/* Karplus-Strong plucked string. Audio thread only. */
static float ks_buf[KS_MAX];
static int ks_len, ks_idx;
static uint32_t ks_seed = 1;

static void pluck(float freq)
{
    ks_len = (int)(g_fs / freq + 0.5f);
    if (ks_len > KS_MAX)
        ks_len = KS_MAX;
    for (int i = 0; i < ks_len; i++) {
        ks_seed = ks_seed * 1664525u + 1013904223u;
        ks_buf[i] = 0.5f * ((float)(ks_seed >> 8) / 8388608.0f - 1.0f);
    }
    ks_idx = 0;
}

static float ks_next(void)
{
    if (ks_len == 0)
        return 0.0f;
    int next = ks_idx + 1 == ks_len ? 0 : ks_idx + 1;
    float s = ks_buf[ks_idx];
    ks_buf[ks_idx] = 0.996f * 0.5f * (s + ks_buf[next]);
    ks_idx = next;
    return s;
}

static void audio_cb(void *user, AudioQueueRef q, AudioQueueBufferRef buf)
{
    static int played;
    (void)user;

    int count = atomic_load(&g_note_count);
    if (count != played) {
        pluck(NOTES[(count - 1) % NUM_NOTES]);
        played = count;
    }

    float *out = buf->mAudioData;
    UInt32 n = buf->mAudioDataBytesCapacity / sizeof(float);
    for (UInt32 i = 0; i < n; i++)
        out[i] = ks_next();

    g_process(g_ctx, out, out, n);

    for (UInt32 i = 0; i < n; i++)
        out[i] *= OUT_GAIN;

    buf->mAudioDataByteSize = n * sizeof(float);
    AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

static void on_sigint(int sig)
{
    (void)sig;
    g_running = 0;
}

int live_host_run(float sample_rate, effect_process_fn process, void *ctx)
{
    g_fs = sample_rate;
    g_process = process;
    g_ctx = ctx;

    AudioStreamBasicDescription fmt = {
        .mSampleRate = sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked,
        .mBytesPerPacket = sizeof(float),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = sizeof(float),
        .mChannelsPerFrame = 1,
        .mBitsPerChannel = 32,
    };

    AudioQueueRef q;
    OSStatus err = AudioQueueNewOutput(&fmt, audio_cb, NULL, NULL, NULL, 0, &q);
    if (err) {
        fprintf(stderr, "AudioQueueNewOutput failed (%d)\n", (int)err);
        return 1;
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(q, BLOCK * sizeof(float), &buf);
        audio_cb(NULL, q, buf);   /* prime with silence */
    }
    err = AudioQueueStart(q, NULL);
    if (err) {
        fprintf(stderr, "AudioQueueStart failed (%d)\n", (int)err);
        return 1;
    }

    /* No SA_RESTART, so Ctrl-C interrupts select() right away. */
    struct sigaction sa = { 0 };
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);

    /* Read single keypresses without Enter; keep Ctrl-C working. */
    struct termios saved;
    bool raw = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved) == 0;
    if (raw) {
        struct termios t = saved;
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }

    printf("Press n to pluck a note, Ctrl-C to quit.\n");
    fflush(stdout);

    bool stdin_open = true;
    int count = 0;
    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        if (stdin_open)
            FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { 0, 100000 };
        if (select(stdin_open ? 1 : 0, &fds, NULL, NULL, &tv) <= 0)
            continue;   /* timeout or interrupted by Ctrl-C */

        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            stdin_open = false;   /* stdin closed; keep playing until Ctrl-C */
            continue;
        }
        if (c == 'n' || c == 'N') {
            count++;
            atomic_store(&g_note_count, count);
            printf("pluck %.0f Hz\n", NOTES[(count - 1) % NUM_NOTES]);
            fflush(stdout);
        }
    }

    if (raw)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    AudioQueueStop(q, true);
    AudioQueueDispose(q, true);
    printf("\nStopped.\n");
    return 0;
}
