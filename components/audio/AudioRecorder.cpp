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
        fprintf(stderr, "No audio devices found!\n");
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

nsresult
AudioRecorder::Init()
{   
    stream = NULL;
    recording = 0;

    PaError err;
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Could not initialize PortAudio! %d\n", err);
        return NS_ERROR_FAILURE;
    }
    
    odata = (ogg_state *)PR_Calloc(1, sizeof(ogg_state));
    vdata = (vorbis_state *)PR_Calloc(1, sizeof(vorbis_state));

    /* Setup our pipe */
    nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
    if (!pipe)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = pipe->Init(
        PR_FALSE, PR_FALSE,
        NUM_CHANNELS * FRAMES_BUFFER * sizeof(float),
        128, nsnull
    );
    if (NS_FAILED(rv))
        return rv;

    pipe->GetInputStream(getter_AddRefs(mPipeIn));
    pipe->GetOutputStream(getter_AddRefs(mPipeOut));
    return NS_OK;
}

AudioRecorder::~AudioRecorder()
{   
    PaError err;
    if ((err = Pa_Terminate()) != paNoError) {
        fprintf(stderr, "JEP Audio:: Could not terminate PortAudio! %d\n", err);
    }
    PR_Free(odata);
    PR_Free(vdata);
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

void
AudioRecorder::Encode(void *data)
{
    int i, ret;
    nsresult rv;
    float **buffer;
    signed char *inp;
    PRUint32 rd, size;
    AudioRecorder *ar = static_cast<AudioRecorder*>(data);
    
    for (;;) {
        buffer = vorbis_analysis_buffer(
            &ar->vdata->vd, FRAMES_BUFFER
        );

        size = FRAMES_BUFFER * NUM_CHANNELS * 2; // 2bytes per uint16
        inp = (signed char *) PR_Calloc(1, size);
        rv = ar->mPipeIn->Read((char *)inp, size, &rd);
        if (rd == 0) {
            fprintf(stderr, "EOF!\n");
            return;
        } else if (rd != size) {
            fprintf(stderr, "Could only read %d from pipe!\n", rd);
            return;
        }
        
        /* uninterleave samples */
        for (i = 0; i < FRAMES_BUFFER; i++){
            buffer[0][i]=((inp[i*4+1]<<8)|
                (0x00ff&(int)inp[i*4]))/32768.f;
            buffer[1][i]=((inp[i*4+3]<<8)|
                (0x00ff&(int)inp[i*4+2]))/32768.f;
        }
        
        PR_Free(inp);
        vorbis_analysis_wrote(&ar->vdata->vd, FRAMES_BUFFER);
        while (vorbis_analysis_blockout(&ar->vdata->vd, &ar->vdata->vb) == 1) {
            vorbis_analysis(&ar->vdata->vb, NULL);
            vorbis_bitrate_addblock(&ar->vdata->vb);

            while (vorbis_bitrate_flushpacket(&ar->vdata->vd, &ar->odata->op)) {
                ogg_stream_packetin(&ar->odata->os, &ar->odata->op);

                for (;;) {
                    ret = ogg_stream_pageout(&ar->odata->os, &ar->odata->og);
                    if (ret == 0)
                        break;
                    fwrite(ar->odata->og.header, 1, ar->odata->og.header_len, ar->outfile);
                    fwrite(ar->odata->og.body, 1, ar->odata->og.body_len, ar->outfile);
                    if (ogg_page_eos(&ar->odata->og))
                        break;
                }
            }
        }
    }
}

int
AudioRecorder::Callback(const void *input, void *output,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *data)
{
    nsresult rv;
    PRUint32 wr, size;
    AudioRecorder *ar = static_cast<AudioRecorder*>(data);

    /* Write to pipe and return quickly */
    size = frames * NUM_CHANNELS * 2; // 2 bytes per uint16
    rv = ar->mPipeOut->Write(
        (const char *)input, size, &wr
    );

    return paContinue;
}

/*
 * Setup Ogg/Vorbis file
 */
nsresult
AudioRecorder::SetupOggVorbis(nsACString& file)
{
    int ret;
    nsresult rv;
    char buf[13];
    nsCAutoString path;
    nsCOMPtr<nsIFile> o;

    /* Assign temporary name */
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(o));
    if (NS_FAILED(rv)) return rv;
    
    MakeRandomString(buf, 8);
    memcpy(buf + 8, ".ogv", 5);
    rv = o->AppendNative(nsDependentCString(buf, 12));
    if (NS_FAILED(rv)) return rv;
    rv = o->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv)) return rv;
    rv = o->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;
    rv = o->Remove(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    /* Open file */
    if (!(outfile = fopen(path.get(), "w+"))) {
        fprintf(stderr, "Could not open OGG file\n");
        return NS_ERROR_FAILURE;
    }
    EscapeBackslash(path);
    file.Assign(path.get(), strlen(path.get()));
    
    vorbis_info_init(&vdata->vi);
    ret = vorbis_encode_init_vbr(&vdata->vi, NUM_CHANNELS, SAMPLE_RATE, SAMPLE_QUALITY);
    if (ret) {
        fprintf(stderr, "Failed vorbis_encode_init!\n");
        return NS_ERROR_FAILURE;
    }
    
    vorbis_comment_init(&vdata->vc);
    vorbis_comment_add_tag(&vdata->vc, "ENCODER", "rainbow");
    vorbis_analysis_init(&vdata->vd, &vdata->vi);
    vorbis_block_init(&vdata->vd, &vdata->vb);
    
    if (ogg_stream_init(&odata->os, rand())) {
        fprintf(stderr, "Failed ogg_stream_init!\n");
        return NS_ERROR_FAILURE;
    }
    
    {
        /* Write out the header */
        ogg_packet header;
        ogg_packet header_comm;
        ogg_packet header_code;
        
        vorbis_analysis_headerout(&vdata->vd, &vdata->vc, &header, &header_comm, &header_code);
        ogg_stream_packetin(&odata->os, &header);
        ogg_stream_packetin(&odata->os, &header_comm);
        ogg_stream_packetin(&odata->os, &header_code);

        while ((ret = ogg_stream_flush(&odata->os, &odata->og)) != 0) {
            fwrite(odata->og.header, 1, odata->og.header_len, outfile) ;
            fwrite(odata->og.body, 1, odata->og.body_len, outfile);
        }
    }

    return NS_OK;
}

/*
 * Start recording to file
 */
NS_IMETHODIMP
AudioRecorder::Start(nsACString& file)
{
    nsresult rv;

    if (recording) {
        fprintf(stderr, "Recording in progress!\n");
        return NS_ERROR_FAILURE;
    }
    rv = SetupOggVorbis(file);
    if (NS_FAILED(rv)) return rv;

    /* Open stream */
    PaError err;
    PaDeviceIndex dev = GetDefaultInputDevice();
    PaStreamParameters inputParameters;    
    inputParameters.device = dev;
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = SAMPLE_FORMAT;
    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo(dev)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,
            SAMPLE_RATE,
            FRAMES_BUFFER,
            paClipOff,
            AudioRecorder::Callback,
            this
    );
    if (err != paNoError) {
        fprintf(stderr, "Could not open stream! %d", err);
    }
    
    /* Start recording */
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "Could not start stream! %d", err);
        return NS_ERROR_FAILURE;
    }

    /* Encode routine on new thread */
    encthr = PR_CreateThread(
        PR_SYSTEM_THREAD,
        AudioRecorder::Encode, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );

    recording = 1;
    return NS_OK;
}

/*
 * Stop stream recording
 */
NS_IMETHODIMP
AudioRecorder::Stop()
{
    if (!recording) {
        fprintf(stderr, "No recording in progress!\n");
        return NS_ERROR_FAILURE;    
    }
    PaError err = Pa_StopStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "Could not close stream!\n");
        return NS_ERROR_FAILURE;
    }
    
    /* Wait for encoder to finish */
    mPipeOut->Close();
    PR_JoinThread(encthr);
    mPipeIn->Close();

    // ADD WRITEBLOCKS()!!!!!!!

    vorbis_block_clear(&vdata->vb);
    vorbis_dsp_clear(&vdata->vd);
    vorbis_comment_clear(&vdata->vc);
    vorbis_info_clear(&vdata->vi);
    ogg_stream_clear(&odata->os);

    fclose(outfile);
    recording = 0;
    return NS_OK;
}
