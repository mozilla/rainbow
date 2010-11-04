/*
 * Interface for video sources to implement. Not really neccessary because we
 * use only portaudio for all platforms, but just in case we switch to something
 * else in the future.
 * TODO: Figure out if and how to do device selection
 */

#include <prlog.h>
#include <nsError.h>
#include <portaudio.h>
#include <nsIOutputStream.h>

#define FRAMES_BUFFER   1024
#define SAMPLE          PRInt16
#define SAMPLE_FORMAT   paInt16

class AudioSource {
public:
    virtual nsresult Stop();
    virtual int GetFrameSize();
    virtual nsresult Start(nsIOutputStream *pipe);
    virtual void SetOptions(int channels, int rate, float quality);
};

/*
 * Portaudio implementation.
 */
class AudioSourceAll : public AudioSource {
public:
    AudioSourceAll();
    ~AudioSourceAll();

    nsresult Stop();
    int GetFrameSize();
    nsresult Start(nsIOutputStream *pipe);
    void SetOptions(int channels, int rate, float quality);

protected:
    int rate;
    int channels;
    float quality;
    PRLogModuleInfo *log;
    nsIOutputStream *output;

    PaStream *stream;
    PaDeviceIndex source;

    static int Callback(
        const void *input, void *output, unsigned long frames,
        const PaStreamCallbackTimeInfo* time,
        PaStreamCallbackFlags flags, void *data
    );

};
