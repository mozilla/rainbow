/*
 * Interface for video sources to implement. Not really neccessary because we
 * use only portaudio for all platforms, but just in case we switch to something
 * else in the future.
 * TODO: Figure out if and how to do device selection
 */
#include <prlog.h>
#include <nsError.h>
#include <nsIOutputStream.h>

/* Defaults */
#define NUM_CHANNELS    1
#define FRAMES_BUFFER   1024

#define SAMPLE          PRInt16
#define SAMPLE_RATE     22050
#define SAMPLE_FORMAT   paInt16
#define SAMPLE_QUALITY  (float)(0.4)

class AudioSource {
public:
    /* Reuse constructor and frame size getter */
    AudioSource(int channels, int rate, float quality);
    int GetFrameSize();

    /* Implement these two. Write 2byte, n-channel audio to pipe */
    virtual nsresult Stop() = 0;
    virtual nsresult Start(nsIOutputStream *pipe) = 0;

protected:
    int rate;
    int channels;
    float quality;
    PRLogModuleInfo *log;

};

