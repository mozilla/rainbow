/*
 * Portaudio implementation
 */
#include "AudioSourcePortaudio.h"

/*
 * Try to intelligently fetch a default audio input device
 */
static PaDeviceIndex
GetDefaultInputDevice()
{
    int i, n;
    PaDeviceIndex def;
    const PaDeviceInfo *deviceInfo;
    
    n = Pa_GetDeviceCount();
    if (n < 0) {
        return paNoDevice;
    }
    
    /* Try default input */
    if ((def = Pa_GetDefaultInputDevice()) != paNoDevice) {
        return def;
    }
    
    /* No luck, iterate and check for API specific input device */
    for (i = 0; i < n; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {
            return i;
        }
    }
    /* No device :( */
    return paNoDevice;
}

AudioSourcePortaudio::AudioSourcePortaudio(int c, int r, float q)
    : AudioSource(c, r, q)
{
    stream = NULL;
    if (Pa_Initialize() != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not initialize PortAudio\n"));
        return;
    }
    source = GetDefaultInputDevice();

    if (source == paNoDevice) {
        PR_LOG(log, PR_LOG_NOTICE, ("No audio devices found!\n"));
    }
}

AudioSourcePortaudio::~AudioSourcePortaudio()
{
    Pa_Terminate();
}

nsresult
AudioSourcePortaudio::Start(nsIOutputStream *pipe)
{
    PaError err;
    PaStreamParameters param;    

    param.device = source;
    param.channelCount = channels;
    param.sampleFormat = SAMPLE_FORMAT;
    param.suggestedLatency =
        Pa_GetDeviceInfo(source)->defaultLowInputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream, &param, NULL, rate, FRAMES_BUFFER,
        paClipOff, AudioSourcePortaudio::Callback, this
    );

    if (err != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not open stream! %d", err));
        return NS_ERROR_FAILURE;
    }

    if (Pa_StartStream(stream) != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not start stream!"));
        return NS_ERROR_FAILURE;
    }
   
    output = pipe;
    return NS_OK;
}

nsresult
AudioSourcePortaudio::Stop()
{
    if (Pa_StopStream(stream) != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not close stream!\n"));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

int
AudioSourcePortaudio::Callback(const void *input, void *output,
    unsigned long frames, const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags, void *data)
{
    nsresult rv;
    PRUint32 wr;
    AudioSourcePortaudio *asa = static_cast<AudioSourcePortaudio*>(data);

    /* Write to pipe and return quickly */
    rv = asa->output->Write(
        (const char *)input, frames * asa->GetFrameSize(), &wr
    );
    return paContinue;
}

