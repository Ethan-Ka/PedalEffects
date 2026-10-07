/*
 * live_host.h - play any effect live through the Mac's speakers.
 *
 * Press n to pluck a note, Ctrl-C to quit. Desktop only (CoreAudio);
 * link with -framework AudioToolbox.
 */
#ifndef LIVE_HOST_H
#define LIVE_HOST_H

#include "audio_io.h"

/* Runs until Ctrl-C. process() is called on the audio thread in blocks of
 * up to AUDIO_MAX_BLOCK samples. Returns 0 on a clean exit. */
int live_host_run(float sample_rate, effect_process_fn process, void *ctx);

#endif /* LIVE_HOST_H */
