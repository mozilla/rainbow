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
 * The Original Code is Video for Jetpack.
 *
 * The Initial Developer of the Original Code is Mozilla Labs.
 * Portions created by the Initial Developer are Copyright (C) 2009-10
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

#include "MediaRecorder.h"

NS_IMPL_ISUPPORTS1(MediaRecorder, IMediaRecorder)

MediaRecorder *MediaRecorder::gMediaRecordingService = nsnull;
MediaRecorder *
MediaRecorder::GetSingleton()
{
    if (gMediaRecordingService) {
        gMediaRecordingService->AddRef();
        return gMediaRecordingService;
    }
    gMediaRecordingService = new MediaRecorder();
    if (gMediaRecordingService) {
        gMediaRecordingService->AddRef();
        if (NS_FAILED(gMediaRecordingService->Init()))
            gMediaRecordingService->Release();
    }
    return gMediaRecordingService;
}

/* Let's get all the static methods out of the way */
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
    /* turn PR_Now() into milliseconds since epoch
     * and salt rand with that.
     */
    double fpTime;
    LL_L2D(fpTime, PR_Now());

    /* use 1e-6, granularity of PR_Now() on the mac is seconds */
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
 * Try to intelligently fetch a default audio input device
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

/*
 * Make a pipe (since NS_NewPipe2 isn't exposed by XPCOM)
 */
static nsresult
MakePipe(nsIAsyncInputStream **pipeIn,
            nsIAsyncOutputStream **pipeOut)
{
    nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
    if (!pipe)
        return NS_ERROR_OUT_OF_MEMORY;

    /* Video frame size > audio frame so we optimize for that */
    int size = WIDTH * HEIGHT * 3 / 2;
    nsresult rv = pipe->Init(
        PR_FALSE, PR_FALSE,
        size, FRAMES_BUFFER, nsnull
    );
    if (NS_FAILED(rv))
        return rv;

    pipe->GetInputStream(pipeIn);
    pipe->GetOutputStream(pipeOut);
    return NS_OK;
}

/*
 * === Here's the meat of the code. Static callbacks & encoder ===
 */

void
MediaRecorder::WriteAudio(void *data)
{
    int ret;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);

    while (vorbis_analysis_blockout(&mr->aState->vd, &mr->aState->vb) == 1) {
        vorbis_analysis(&mr->aState->vb, NULL);
        vorbis_bitrate_addblock(&mr->aState->vb);
        while (vorbis_bitrate_flushpacket(
                &mr->aState->vd, &mr->aState->op)) {
            ogg_stream_packetin(&mr->aState->os, &mr->aState->op);

            for (;;) {
                ret = ogg_stream_pageout(&mr->aState->os, &mr->aState->og);
                if (ret == 0)
                    break;
                fwrite(mr->aState->og.header, mr->aState->og.header_len,
                    1, mr->outfile);
                fwrite(mr->aState->og.body, mr->aState->og.body_len,
                    1, mr->outfile);
                if (ogg_page_eos(&mr->aState->og))
                    break;
            }
        }
    }
}

/*
 * Encoder, runs in a thread
 */
void
MediaRecorder::Encode(void *data)
{
    int i, j;
    nsresult rv;
    PRUint32 rd = 0;

    char *a_frame;
    float **a_buffer;
    unsigned char *v_frame;
    th_ycbcr_buffer v_buffer;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);

    /* Alright, so we are multiplexing audio with video. We first fetch 1 frame
     * of video from the pipe, encode it and then follow up with packets of
     * audio. We want the audio and video packets to be as close to each other
     * as possible (timewise) to make playback synchronized.
     *
     * The generic formula to decide how many bytes of audio we must write per
     * frame of video will be: (SAMPLE_RATE / FPS) * AUDIO_FRAME_SIZE;
     *
     * This works out to 8820 bytes of 22050Hz audio (2205 frames)
     * per frame of video @ 10 fps (these are the default values).
     *
     * Sure, this assumes that the audio and video recordings start at exactly
     * the same time. While that may not be true if you look at the
     * millisecond level, our brain does an amazing job of approximating and
     * WANTS to see synchronized audio and video. It all works out just fine :)
     *
     * OR DOES IT? FIXME!
     *
     * TODO: Figure out if PR_Calloc will be more efficient if we call it for
     * storing more than just 1 frame at a time. For instance, we can wait
     * a second and encode 10 frames of video and 88200 bytes of audio per
     * run of the loop?
     */
    int a_frame_len = SAMPLE_RATE/(FPS_N/FPS_D);
    int a_frame_size = a_frame_len * mr->aState->fsize;
            
    for (;;) {

        if (mr->v_rec) {

        /* Make sure we get 1 full frame of video */
        v_frame = (unsigned char *) PR_Calloc(1, mr->vState->fsize);

        do mr->vState->vPipeIn->Available(&rd);
            while ((rd < (PRUint32)mr->vState->fsize) && !mr->v_stp);
        rv = mr->vState->vPipeIn->Read((char *)v_frame, mr->vState->fsize, &rd);

        if (rd == 0) {
            /* EOF. If there's audio we need to finish it. Goto: gasp! */
            PR_Free(v_frame);
            if (mr->a_rec)
                goto audio_enc;
            else
                return;
        } else if (rd != (PRUint32)mr->vState->fsize) {
            /* Now, we're in a pickle. HOW TO FIXME? */
            fprintf(stderr,
                "only read %u of %d from video pipe\n", rd, mr->vState->fsize);
            PR_Free(v_frame);
            return;
        }

        /* Convert i420 to YCbCr */
        v_buffer[0].width = WIDTH;
        v_buffer[0].stride = WIDTH;
        v_buffer[0].height = HEIGHT;

        v_buffer[1].width = (WIDTH >> 1);
        v_buffer[1].height = (HEIGHT >> 1);
        v_buffer[1].stride = v_buffer[1].width;

        v_buffer[2].width = v_buffer[1].width;
        v_buffer[2].height = v_buffer[1].height;
        v_buffer[2].stride = v_buffer[1].stride;

        v_buffer[0].data = v_frame;
        v_buffer[1].data = v_frame + WIDTH * HEIGHT;
        v_buffer[2].data = v_buffer[1].data + WIDTH * HEIGHT / 4;

        /* Encode 'er up */
        if (th_encode_ycbcr_in(mr->vState->th, v_buffer) != 0) {
            fprintf(stderr, "Could not encode frame!\n");
            return;
        }
        if (!th_encode_packetout(mr->vState->th, 0, &mr->vState->op)) {
            fprintf(stderr, "Could not read packet!\n");
            return;
        }

        /* Write to canvas, if needed. Move to another thread? */
        if (mr->vState->vCanvas) {
            unsigned char *rgb = (unsigned char *)
                PR_Calloc(1, WIDTH * HEIGHT * 4);

            /* Canvas wants rgba. */
            vidcap_i420_to_rgb32(
                WIDTH, HEIGHT,
                (const char *)v_frame, (char *)rgb
            );
            /* That didn't give us what we want. Reconfigure. */
            unsigned char tmp = 0;
            for (int j = 0; j < WIDTH * HEIGHT * 4; j += 4) {
                tmp = rgb[j+2];
                rgb[j+2] = rgb[j];
                rgb[j] = tmp;
            }
            rv = mr->vState->vCanvas->PutImageData_explicit(
                0, 0, WIDTH, HEIGHT, rgb, WIDTH * HEIGHT * 4
            );
            PR_Free((void *)rgb);
        }

        ogg_stream_packetin(&mr->vState->os, &mr->vState->op);
        while (ogg_stream_pageout(&mr->vState->os, &mr->vState->og)) {
            fwrite(mr->vState->og.header, mr->vState->og.header_len,
                1, mr->outfile);
            fwrite(mr->vState->og.body, mr->vState->og.body_len,
                1, mr->outfile);
        }
        PR_Free(v_frame);

        }

audio_enc:
    
        if (mr->a_rec) {
             
        /* Make sure we get enough frames in unless we're at the end */
        a_frame = (char *) PR_Calloc(1, a_frame_size);

        do mr->aState->aPipeIn->Available(&rd);
            while ((rd < (PRUint32)a_frame_size) && !mr->a_stp);
        rv = mr->aState->aPipeIn->Read((char *)a_frame, a_frame_size, &rd);

        if (rd == 0) {
            /* EOF. We're done. */
            PR_Free(a_frame);
            return;
        } else if (rd != (PRUint32)a_frame_size) {
            /* Hmm. I sure hope this is the end of the recording. */
            PR_Free(a_frame);
            if (!mr->a_stp)
                fprintf(stderr,
                    "only read %u of %d from audio pipe\n", rd, a_frame_size);
            return;
        }
          
        /* Uninterleave samples. Alternatively, have portaudio do this? */
        a_buffer = vorbis_analysis_buffer(&mr->aState->vd, a_frame_size);
        for (i = 0; i < a_frame_len; i++){
            for (j = 0; j < NUM_CHANNELS; j++) {
                a_buffer[j][i] =
                    (float)((a_frame[i*mr->aState->fsize+((j*2)+1)]<<8) |
                        ((0x00ff&(int)a_frame[i*mr->aState->fsize+(j*2)]))) /
                            32768.f;
            }
        }
        
        /* Tell libvorbis to do its thing */
        vorbis_analysis_wrote(&mr->aState->vd, a_frame_len);
        MediaRecorder::WriteAudio(data);
        PR_Free(a_frame);

        }

        /* We keep looping until we reach EOF on either pipe */
        fflush(mr->outfile);
    }
}

/*
 * NOTE: The following callbacks are run on high prority
 * threads. Real-time constraints apply. Don't do anything funky
 * inside (like calling non-thread-safe functions, or file I/O)
 * Return as quickly as possible! We use nsIPipe to store data.
 */
int
MediaRecorder::VideoCallback(vidcap_src *src, void *data,
    struct vidcap_capture_info *video)
{
    nsresult rv;
    PRUint32 wr;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);
    
    /* Write to pipe and return quickly */
    rv = mr->vState->vPipeOut->Write(
        (const char *)video->video_data,
        video->video_data_size, &wr
    );
    return 0;
}

int
MediaRecorder::AudioCallback(const void *input, void *output,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *data)
{
    nsresult rv;
    PRUint32 wr, size;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);

    /* Write to pipe and return quickly */
    size = frames * mr->aState->fsize;
    rv = mr->aState->aPipeOut->Write(
        (const char *)input, size, &wr
    );

    return paContinue;
}

/*
 * === Class methods begin now! ===
 */
nsresult
MediaRecorder::Init()
{
    a_rec = PR_FALSE;
    v_rec = PR_FALSE;
    aState = (Audio *)PR_Calloc(1, sizeof(Audio));
    vState = (Video *)PR_Calloc(1, sizeof(Video));

    /* Setup frame sizes */
    vState->fsize = WIDTH * HEIGHT * 3 / 2;
    aState->fsize = sizeof(SAMPLE) * NUM_CHANNELS;

    /* Setup video */
    int nDevices = 0;
    struct vidcap_sapi_info sapi_info;
    if (!(vState->state = vidcap_initialize())) {
        fprintf(stderr, "Could not initialize vidcap, aborting!\n");
        return NS_ERROR_FAILURE;
    }
    if (!(vState->sapi = vidcap_sapi_acquire(vState->state, 0))) {
        fprintf(stderr, "Failed to acquire default sapi\n");
        return NS_ERROR_FAILURE;
    }
    if (vidcap_sapi_info_get(vState->sapi, &sapi_info)) {
        fprintf(stderr, "Failed to get default sapi info\n");
        return NS_ERROR_FAILURE;
    }
    nDevices = vidcap_src_list_update(vState->sapi);
    if (nDevices < 0) {
        fprintf(stderr, "Failed vidcap_src_list_update()\n");
        return NS_ERROR_FAILURE;
    } else if (nDevices == 0) {
        /* No video capture device available */
        PR_Free(vState);
        vState = nsnull;
    } else {
        if (!(vState->sources = (struct vidcap_src_info *)
            PR_Calloc(nDevices, sizeof(struct vidcap_src_info)))) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (vidcap_src_list_get(
                vState->sapi, nDevices, vState->sources)) {
            PR_Free(vState->sources);
            fprintf(stderr, "Failed vidcap_src_list_get()\n");
            return NS_ERROR_FAILURE;
        }
    }
    
    /* Setup audio */
    aState->stream = NULL;
    if (Pa_Initialize() != paNoError) {
        fprintf(stderr, "Could not initialize PortAudio!\n");
        return NS_ERROR_FAILURE;
    }
    aState->source = GetDefaultInputDevice();
    
    if (aState->source == paNoDevice) {
        /* No audio capture device available */
        PR_Free(aState);
        aState = nsnull;
    }    
    return NS_OK;   
}

MediaRecorder::~MediaRecorder()
{
    Pa_Terminate();
    vidcap_sapi_release(vState->sapi);
    vidcap_destroy(vState->state);

    PR_Free(vState->sources);
    PR_Free(vState);
    PR_Free(aState);

    gMediaRecordingService = nsnull;
}

/*
 * Theora beginning of stream
 */
nsresult
MediaRecorder::SetupTheoraBOS()
{
    if (ogg_stream_init(&vState->os, rand())) {
        fprintf(stderr, "Failed ogg_stream_init!\n");
        return NS_ERROR_FAILURE;
    }
    
    th_info_init(&vState->ti);
    /* Must be multiples of 16 */
    vState->ti.frame_width = ((WIDTH + 15) >> 4) << 4;
    vState->ti.frame_height = ((HEIGHT + 15) >> 4) << 4;
    vState->ti.pic_width = WIDTH;
    vState->ti.pic_height = HEIGHT;
    vState->ti.pic_x = 0;
    vState->ti.pic_y = 0;
    
    /* Too fast? Why? */
    vState->ti.fps_numerator = FPS_N;
    vState->ti.fps_denominator = FPS_D;
    vState->ti.aspect_numerator = 0;
    vState->ti.aspect_denominator = 0;
    vState->ti.colorspace = TH_CS_UNSPECIFIED;
    vState->ti.pixel_fmt = TH_PF_420;
    vState->ti.target_bitrate = 0;
    vState->ti.quality = 48;
    
    vState->th = th_encode_alloc(&vState->ti);
    th_info_clear(&vState->ti);
    
    /* Header init */
    th_comment_init(&vState->tc);
    th_comment_add_tag(&vState->tc, (char *)"ENCODER", (char *)"rainbow");
    if (th_encode_flushheader(
        vState->th, &vState->tc, &vState->op) <= 0) {
        fprintf(stderr,"Internal Theora library error.\n");
        return NS_ERROR_FAILURE;
    }
    th_comment_clear(&vState->tc);
    
    ogg_stream_packetin(&vState->os, &vState->op);
    if (ogg_stream_pageout(&vState->os, &vState->og) != 1) {
        fprintf(stderr,"Internal Ogg library error.\n");
        return NS_ERROR_FAILURE;
    }
    fwrite(vState->og.header, 1, vState->og.header_len, outfile);
    fwrite(vState->og.body, 1, vState->og.body_len, outfile);

    return NS_OK;
}

/*
 * Theora stream headers
 */
nsresult
MediaRecorder::SetupTheoraHeaders()
{
    int ret;

    /* Create rest of the theora headers */
    for (;;) {
        ret = th_encode_flushheader(
            vState->th, &vState->tc, &vState->op
        );
        if (ret < 0){
            fprintf(stderr,"Internal Theora library error.\n");
            return NS_ERROR_FAILURE;
        } else if (!ret) break;
        ogg_stream_packetin(&vState->os, &vState->op);
    }
    /* Flush the rest of theora headers. */
    for (;;) {
        ret = ogg_stream_flush(&vState->os, &vState->og);
        if (ret < 0){
            fprintf(stderr,"Internal Ogg library error.\n");
            return NS_ERROR_FAILURE;
        }
        if (ret == 0) break;
        fwrite(vState->og.header, 1, vState->og.header_len, outfile);
        fwrite(vState->og.body, 1, vState->og.body_len, outfile);
    }

    return NS_OK;
}

/*
 * Vorbis beginning of stream
 */
nsresult
MediaRecorder::SetupVorbisBOS()
{
    int ret;
    if (ogg_stream_init(&aState->os, rand())) {
        fprintf(stderr, "Failed ogg_stream_init!\n");
        return NS_ERROR_FAILURE;
    }

    vorbis_info_init(&aState->vi);
    ret = vorbis_encode_init_vbr(
        &aState->vi, NUM_CHANNELS, SAMPLE_RATE, SAMPLE_QUALITY
    );
    if (ret) {
        fprintf(stderr, "Failed vorbis_encode_init!\n");
        return NS_ERROR_FAILURE;
    }
    
    vorbis_comment_init(&aState->vc);
    vorbis_comment_add_tag(&aState->vc, "ENCODER", "rainbow");
    vorbis_analysis_init(&aState->vd, &aState->vi);
    vorbis_block_init(&aState->vd, &aState->vb);
    
    {
        ogg_packet header;
        ogg_packet header_comm;
        ogg_packet header_code;
        
        vorbis_analysis_headerout(
            &aState->vd, &aState->vc,
            &header, &header_comm, &header_code
        );
        ogg_stream_packetin(&aState->os, &header);
        ogg_stream_packetin(&aState->os, &header_comm);
        ogg_stream_packetin(&aState->os, &header_code);

        if (ogg_stream_pageout(&aState->os, &aState->og) != 1) {
            fprintf(stderr,"Internal Ogg library error.\n");
            return NS_ERROR_FAILURE;
        }
        fwrite(aState->og.header, 1, aState->og.header_len, outfile);
        fwrite(aState->og.body, 1, aState->og.body_len, outfile);
    }

    return NS_OK;
}

/*
 * Setup vorbis stream headers
 */
nsresult
MediaRecorder::SetupVorbisHeaders()
{
    int ret;
    while ((ret = ogg_stream_flush(&aState->os, &aState->og)) != 0) {
        fwrite(aState->og.header, 1, aState->og.header_len, outfile);
        fwrite(aState->og.body, 1, aState->og.body_len, outfile);
    }

    return NS_OK;
}

/*
 * Create a temporary file to dump to
 */
nsresult
MediaRecorder::CreateFile(nsIDOMHTMLInputElement *input, nsACString &file)
{
    nsresult rv;
    char buf[13];
    const char *name;
    nsCAutoString path;
    nsCOMPtr<nsIFile> o;

    /* Assign temporary name */
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

    /* Open file */
    name = path.get();
    if (!(outfile = fopen(name, "w+"))) {
        fprintf(stderr, "Could not open OGG file\n");
        return NS_ERROR_FAILURE;
    }
    EscapeBackslash(path);


    /* We get a DOMFile in this convoluted manner because nsDOMFile is not
     * public. See bug #607114
     */
    NS_ConvertUTF8toUTF16 unipath(name);
    const PRUnichar *arr = unipath.get();
    rv = input->MozSetFileNameArray(&arr, 1);
    if (NS_FAILED(rv)) return rv;

    file.Assign(name, strlen(name));
    return rv;
}

/*
 * Start recording to file
 */
NS_IMETHODIMP
MediaRecorder::Start(
    PRBool audio, PRBool video,
    nsIDOMHTMLInputElement *input,
    nsIDOMCanvasRenderingContext2D *ctx,
    nsACString &file
)
{
    nsresult rv;
    if (a_rec || v_rec) {
        fprintf(stderr, "Recording in progress!\n");
        return NS_ERROR_FAILURE;
    }
    if (audio && (aState == nsnull)) {
        fprintf(stderr, "Audio requested but no devices found!\n");
        return NS_ERROR_FAILURE;
    }
    if (video && (vState == nsnull)) {
        fprintf(stderr, "Video requested but no devices found!\n");
        return NS_ERROR_FAILURE;
    }

    rv = CreateFile(input, file);
    if (NS_FAILED(rv)) return rv;

    /* Get ready for video! */
    if (video) {
        struct vidcap_fmt_info fmt_info;
        fmt_info.width = WIDTH;
        fmt_info.height = HEIGHT;
        /* FIXME: What would be the best format to record in? */
        fmt_info.fourcc = VIDCAP_FOURCC_I420;
        fmt_info.fps_numerator = FPS_N;
        fmt_info.fps_denominator = FPS_D;

        if (!(vState->source = vidcap_src_acquire(
            vState->sapi, &vState->sources[0]))) {
            fprintf(stderr, "Failed vidcap_src_acquire()\n");
            return NS_ERROR_FAILURE;
        }
        if (video && ctx) {
            vState->vCanvas = ctx;
        }
        
        if (vidcap_format_bind(vState->source, &fmt_info)) {
            fprintf(stderr, "Failed vidcap_format_bind()\n");
            return NS_ERROR_FAILURE;
        }
        SetupTheoraBOS();

        rv = MakePipe(
            getter_AddRefs(vState->vPipeIn),
            getter_AddRefs(vState->vPipeOut)
        );
        if (NS_FAILED(rv)) return rv;
    }

    /* Get ready for audio! */
    if (audio) {
        PaError err;
        PaStreamParameters param;    

        param.device = aState->source;
        param.channelCount = NUM_CHANNELS;
        param.sampleFormat = SAMPLE_FORMAT;
        param.suggestedLatency =
            Pa_GetDeviceInfo(aState->source)->defaultLowInputLatency;
        param.hostApiSpecificStreamInfo = NULL;

        err = Pa_OpenStream(
                &aState->stream,
                &param,
                NULL,
                SAMPLE_RATE,
                FRAMES_BUFFER,
                paClipOff,
                MediaRecorder::AudioCallback,
                this
        );

        if (err != paNoError) {
            fprintf(stderr, "Could not open stream! %d", err);
            return NS_ERROR_FAILURE;
        }
        SetupVorbisBOS();

        rv = MakePipe(
            getter_AddRefs(aState->aPipeIn),
            getter_AddRefs(aState->aPipeOut)
        );
        if (NS_FAILED(rv)) return rv;
    }

    /* Let's DO this. */
    if (video) {
        SetupTheoraHeaders();
        if (vidcap_src_capture_start(
                vState->source, MediaRecorder::VideoCallback, this)) {
            fprintf(stderr, "Failed vidcap_src_capture_start()\n");
            return NS_ERROR_FAILURE;
        }
        v_rec = PR_TRUE;
        v_stp = PR_FALSE;
    }
    if (audio) {
        SetupVorbisHeaders();
        if (Pa_StartStream(aState->stream) != paNoError) {
            fprintf(stderr, "Could not start stream!");
            return NS_ERROR_FAILURE;
        }
        a_rec = PR_TRUE;
        a_stp = PR_FALSE;
    }


    /* Encode thread */
    encoder = PR_CreateThread(
        PR_SYSTEM_THREAD,
        MediaRecorder::Encode, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );

    return NS_OK;
}

/*
 * Stop recording
 */
NS_IMETHODIMP
MediaRecorder::Stop()
{
    if (!a_rec && !v_rec) {
        fprintf(stderr, "No recording in progress!\n");
        return NS_ERROR_FAILURE;    
    }

    if (v_rec) {
        if (vidcap_src_capture_stop(vState->source)) {
            fprintf(stderr, "Failed vidcap_src_capture_stop()\n");
            return NS_ERROR_FAILURE;
        }
        vidcap_src_release(vState->source);
    }
    
    if (a_rec) {
        if (Pa_StopStream(aState->stream) != paNoError) {
            fprintf(stderr, "Could not close stream!\n");
            return NS_ERROR_FAILURE;
        }
    }

    /* Wait for encoder to finish */
    if (v_rec) {
        v_stp = PR_TRUE;
        vState->vPipeOut->Close();
    }
    if (a_rec) {
        a_stp = PR_TRUE;
        aState->aPipeOut->Close();
    }

    PR_JoinThread(encoder);
    
    if (v_rec) {
        vState->vPipeIn->Close();
        th_encode_free(vState->th);

        /* Video trailer */
        if (ogg_stream_flush(&vState->os, &vState->og)) {
            fwrite(vState->og.header, vState->og.header_len, 1, outfile);
            fwrite(vState->og.body, vState->og.body_len, 1, outfile);
        }

        ogg_stream_clear(&vState->os);
        v_rec = PR_FALSE;
    }
    
    if (a_rec) {
        aState->aPipeIn->Close();
    
        /* Audio trailer */
        vorbis_analysis_wrote(&aState->vd, 0);
        MediaRecorder::WriteAudio(this);

        vorbis_block_clear(&aState->vb);
        vorbis_dsp_clear(&aState->vd);
        vorbis_comment_clear(&aState->vc);
        vorbis_info_clear(&aState->vi);
        ogg_stream_clear(&aState->os);
        a_rec = PR_FALSE;
    }
 
    /* GG */
    fclose(outfile);
    return NS_OK;
}

