/*
 * Portaudio implementation
 */
#include "AudioSource.h"
#include <portaudio.h>

class AudioSourcePortaudio : public AudioSource {
public:
    AudioSourcePortaudio(int c, int r, float q);
    ~AudioSourcePortaudio();

    nsresult Stop();
    nsresult Start(nsIOutputStream *pipe);

protected:
    PaStream *stream;
    PaDeviceIndex source;
    nsIOutputStream *output;

    static int Callback(
        const void *input, void *output, unsigned long frames,
        const PaStreamCallbackTimeInfo* time,
        PaStreamCallbackFlags flags, void *data
    );

};

