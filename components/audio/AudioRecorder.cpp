/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Audio Recorder.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Labs
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anant Narayanan <anant@kix.in>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "AudioRecorder.h"

NS_IMPL_ISUPPORTS1(AudioRecorder, IAudioRecorder)

AudioRecorder *AudioRecorder::gAudioRecordingService = nsnull;

AudioRecorder *
AudioRecorder::GetSingleton()
{
    if (gAudioRecordingService) {
        NS_ADDREF(gAudioRecordingService);
        return gAudioRecordingService;
    }
    
    gAudioRecordingService = new AudioRecorder();
    if (gAudioRecordingService) {
        NS_ADDREF(gAudioRecordingService);
        if (NS_FAILED(gAudioRecordingService->Init()))
            NS_RELEASE(gAudioRecordingService);
    }
    
    return gAudioRecordingService;
}

nsresult
AudioRecorder::Init()
{   
    stream = NULL;
    recording = 0;

    PaError err;
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not initialize PortAudio! %d\n", err);
        return NS_ERROR_FAILURE;
    }
    
    return NS_OK;
}

AudioRecorder::~AudioRecorder()
{   
    PaError err;
    if ((err = Pa_Terminate()) != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not terminate PortAudio! %d\n", err);
    }
    
    gAudioRecordingService = nsnull;
}

#define TABLE_SIZE 36
static const char table[] = {
    'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9' 
};

/*
 * This code is ripped from profile/src/nsProfile.cpp and is further
 * duplicated in uriloader/exthandler.  this should probably be moved
 * into xpcom or some other shared library.
 */ 
static void
MakeRandomString(char *buf, PRInt32 bufLen)
{
    // turn PR_Now() into milliseconds since epoch
    // and salt rand with that.
    double fpTime;
    LL_L2D(fpTime, PR_Now());

    // use 1e-6, granularity of PR_Now() on the mac is seconds
    srand((uint)(fpTime * 1e-6 + 0.5));   
    PRInt32 i;
    for (i=0;i<bufLen;i++) {
        *buf++ = table[rand()%TABLE_SIZE];
    }
    *buf = 0;
}

/*
 * This replaces \ with \\ so that Windows paths are sane
 */
static void
EscapeBackslash(nsACString& str)
{
	const char *sp;
	const char *mp = "\\";
	const char *np = "\\\\";

	PRUint32 sl;
	PRUint32 ml = 1;
	PRUint32 nl = 2;

	sl = NS_CStringGetData(str, &sp);
	for (const char* iter = sp; iter <= sp + sl - ml; ++iter) {
	    if (memcmp(iter, mp, ml) == 0) {
            PRUint32 offset = iter - sp;
            NS_CStringSetDataRange(str, offset, ml, np, nl);
            sl = NS_CStringGetData(str, &sp);
            iter = sp + offset + nl - 1;
	    }
	}
}

/*
 * Try to intelligently fetch a default input device
 */
static PaDeviceIndex
GetDefaultInputDevice()
{
    int i, numDevices;
    PaDeviceIndex def;
    const PaDeviceInfo *deviceInfo;
    
    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        fprintf(stderr, "JEP Audio:: No audio devices found!\n");
        return paNoDevice;
    }
    
    /* Try default input */
    if ((def = Pa_GetDefaultInputDevice()) != paNoDevice) {
        return def;
    }
    
    /* No luck, iterate and check for API specific input device */
    for (i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {
            return i;
        }
    }
    
    /* No device :( */
    return paNoDevice;
}

int
AudioRecorder::RecordCallback(const void *input, void *output,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData)
{
    PRUint32 written;
    nsIAsyncOutputStream *op = static_cast<AudioRecorder*>(userData)->mPipeOut;

    if (input != NULL) {
        op->Write((const char *)input,
            (PRUint32)(sizeof(SAMPLE) * NUM_CHANNELS * framesPerBuffer),
                &written);
    }
    
    return paContinue;
}

int
AudioRecorder::RecordToFileCallback(const void *input, void *output,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData)
{
    SNDFILE *out = static_cast<AudioRecorder*>(userData)->outfile;

    if (input != NULL) {
        sf_writef_int(out, (const SAMPLE *)input, framesPerBuffer);
    }
    
    return paContinue;
}

/*
 * Start recording
 */
NS_IMETHODIMP
AudioRecorder::Start(nsIAsyncInputStream** out)
{
    if (recording) {
        fprintf(stderr, "JEP Audio:: Recording in progress!\n");
        return NS_ERROR_FAILURE;
    }

    /* Create pipe: NS_NewPipe2 is not exported by XPCOM */
    nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
    if (!pipe)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = pipe->Init(PR_TRUE, PR_TRUE, 0, PR_UINT32_MAX, NULL);
    if (NS_FAILED(rv)) return rv;

    pipe->GetInputStream(&mPipeIn);
    pipe->GetOutputStream(&mPipeOut);

    recording = 1;
    *out = mPipeIn;

    /* Check for audio input device */
    PaDeviceIndex dev;
    dev = GetDefaultInputDevice();
    if (dev == paNoDevice) {
        fprintf(stderr, "JEP Audio:: Could not find input device!\n");
        return NS_ERROR_UNEXPECTED;
    }
    
    /* Open stream */
    PaError err;
    PaStreamParameters inputParameters;    
    inputParameters.device = dev;
    inputParameters.channelCount = 2;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo(dev)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,
            this->RecordCallback,
            this
    );
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not open stream! %d", err);
    }
    
    /* Start recording */
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not start stream! %d", err);
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/*
 * Start recording to file
 */
NS_IMETHODIMP
AudioRecorder::StartRecordToFile(nsACString& file)
{
    if (recording) {
        fprintf(stderr, "JEP Audio:: Recording in progress!\n");
        return NS_ERROR_FAILURE;
    }

    /* Check for audio input device */
    nsresult rv;
    PaDeviceIndex dev;
	nsCOMPtr<nsIFile> o;

    dev = GetDefaultInputDevice();
    if (dev == paNoDevice) {
        fprintf(stderr, "JEP Audio:: Could not find input device!\n");
        return NS_ERROR_UNEXPECTED;
    }
    
    /* Allocate OGG file */
    char buf[13];
    nsCAutoString path;

    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(o));
    if (NS_FAILED(rv)) return rv;

    MakeRandomString(buf, 8);
    memcpy(buf + 8, ".ogg", 5);
    rv = o->AppendNative(nsDependentCString(buf, 12));
    if (NS_FAILED(rv)) return rv;
    rv = o->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv)) return rv;
    rv = o->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;
    rv = o->Remove(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    /* Open file in libsndfile */
    SF_INFO info;
    info.channels = NUM_CHANNELS;
    info.samplerate = SAMPLE_RATE;
    info.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;

    if (!(outfile = sf_open(path.get(), SFM_WRITE, &info))) {
        sf_perror(NULL);
        return NS_ERROR_FAILURE;
    }

    EscapeBackslash(path);
	file.Assign(path.get(), strlen(path.get()));

    /* Open stream */
    PaError err;
    PaStreamParameters inputParameters;    
    inputParameters.device = dev;
    inputParameters.channelCount = 2;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo(dev)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,
            this->RecordToFileCallback,
            this
    );
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not open stream! %d", err);
    }
    
    /* Start recording */
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not start stream! %d", err);
        return NS_ERROR_FAILURE;
    }
	recording = 2;
    return NS_OK;
}

/*
 * Stop stream recording
 */
NS_IMETHODIMP
AudioRecorder::Stop()
{
    if (!recording) {
        fprintf(stderr, "JEP Audio:: No recording in progress!\n");
        return NS_ERROR_FAILURE;    
    }
    
    PaError err = Pa_StopStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not close stream!\n");
        return NS_ERROR_FAILURE;
    }
    
	if (recording == 1) {
		mPipeOut->Close();
	}
    recording = 0;
    return NS_OK;
}
