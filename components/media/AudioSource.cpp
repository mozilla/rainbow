/*
 * Common AudioSource implementation
 */
#include "AudioSource.h"

AudioSource::AudioSource(int c, int r, float q)
{
    rate = r;
    quality = q;
    channels = c;

    /* Setup logger */
    log = PR_NewLogModule("VideoSource");
}

int
AudioSource::GetFrameSize()
{
    return sizeof(SAMPLE) * channels;
}

