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

/*
 * Special runnable object to dispatch JS callback on main thread
 */
class MediaCallback : public nsRunnable {
public:
    MediaCallback(nsIMediaStateObserver *obs, const char *msg, const char *arg) {
        m_Obs = obs;
        strcpy(m_Msg, msg);
        strcpy(m_Arg, arg);
    }
    
    NS_IMETHOD Run() {
        return m_Obs->OnStateChange((char *)m_Msg, (char *)m_Arg);
    }
    
private:
    nsIMediaStateObserver *m_Obs;
    char m_Msg[128]; char m_Arg[256];
};

void
MediaRecorder::WriteAudio()
{
    int ret;
    nsresult rv;
    PRUint32 wr;

    while (vorbis_analysis_blockout(&aState->vd, &aState->vb) == 1) {
        vorbis_analysis(&aState->vb, NULL);
        vorbis_bitrate_addblock(&aState->vb);
        while (vorbis_bitrate_flushpacket(
                &aState->vd, &aState->op)) {
            ogg_stream_packetin(&aState->os, &aState->op);

            for (;;) {
                ret = ogg_stream_pageout(&aState->os, &aState->og);
                if (ret == 0)
                    break;

                rv = WriteData(aState->og.header, aState->og.header_len, &wr);
                rv = WriteData(aState->og.body, aState->og.body_len, &wr);

                if (ogg_page_eos(&aState->og))
                    break;
            }
        }
    }
}

nsresult
MediaRecorder::WriteData(unsigned char *data, PRUint32 len, PRUint32 *wr)
{
    return pipeStream->Write((const char*)data, len, wr);
}

/*
 * Get an audio packet from the pipe.
 */
PRInt16 *
MediaRecorder::GetAudioPacket(PRInt32 *len, PRInt32 *time_s, PRInt32 *time_us)
{
    nsresult rv;
    PRUint32 rd;
    PRInt16 *a_frames;
    
    /* Get audio frame header */
    rv = aState->aPipeIn->Read((char *)time_s, sizeof(PRInt32), &rd);
    rv = aState->aPipeIn->Read((char *)time_us, sizeof(PRInt32), &rd);
    rv = aState->aPipeIn->Read((char *)len, sizeof(PRUint32), &rd);
    fprintf(stderr, "Got audio frame for %d bytes at %d :: %d\n", *len, *time_s, *time_us);
    
    a_frames = (PRInt16 *) PR_Calloc(*len, sizeof(PRUint8));
    do aState->aPipeIn->Available(&rd);
        while ((rd < (PRUint32)*len) && !a_stp);
    rv = aState->aPipeIn->Read((char *)a_frames, *len, &rd);
    
    if (rd == 0) {
        /* EOF. We're done. */
        PR_Free(a_frames);
        return NULL;
    } else if (rd != (PRUint32)*len) {
        /* Hmm. I sure hope this is the end of the recording. */
        if (!a_stp) {
            PR_LOG(log, PR_LOG_NOTICE,
                ("only read %u of %d from audio pipe\n", rd, *len));
        }
        PR_Free(a_frames);
        return NULL;
    }
    
    return a_frames;
}

/*
 * Encode length bytes of audio from the packet into Ogg stream
 */
PRBool
MediaRecorder::EncodeAudio(PRInt16 *a_frames, int len)
{
    int i, j, n;
    float **a_buffer;

    /* Uninterleave samples */
    n = len / aState->backend->GetFrameSize();
    a_buffer = vorbis_analysis_buffer(&aState->vd, n);
    for (i = 0; i < n; i++){
        for (j = 0; j < (int)params->chan; j++) {
            a_buffer[j][i] = (float)((float)a_frames[i+j] / 32768.f);
        }
    }

    /* Tell libvorbis to do its thing */
    vorbis_analysis_wrote(&aState->vd, n);
    WriteAudio();
    PR_Free(a_frames);
    
    return PR_TRUE;
}

/*
 * Get an audio packet from the pipe.
 */
PRUint8 *
MediaRecorder::GetVideoPacket(PRInt32 *len, PRInt32 *time_s, PRInt32 *time_us)
{
    nsresult rv;
    PRUint32 rd;
    PRUint8 *v_frame;
    
    /* Get video frame header */
    rv = vState->vPipeIn->Read((char *)time_s, sizeof(PRInt32), &rd);
    rv = vState->vPipeIn->Read((char *)time_us, sizeof(PRInt32), &rd);
    rv = vState->vPipeIn->Read((char *)len, sizeof(PRUint32), &rd);
    fprintf(stderr, "Got video frame for %d bytes at %d :: %d\n", *len, *time_s, *time_us);
    
    v_frame = (PRUint8 *)PR_Calloc(*len, sizeof(PRUint8));
    do vState->vPipeIn->Available(&rd);
        while ((rd < (PRUint32)*len) && !v_stp);
    rv = vState->vPipeIn->Read((char *)v_frame, *len, &rd);
    
    if (rd == 0) {
        PR_Free(v_frame);
        return NULL;
    } else if (rd != (PRUint32)*len) {
        PR_LOG(log, PR_LOG_NOTICE,
            ("only read %u of %d from video pipe\n", rd, *len));
        PR_Free(v_frame);
        return NULL;
    }
    
    return v_frame;
}

/*
 * Encode length bytes of video from the packet into Ogg stream
 */
PRBool
MediaRecorder::EncodeVideo(PRUint8 *v_frame, int len)
{
    nsresult rv;
    PRUint32 wr;
    th_ycbcr_buffer v_buffer;

    /* Convert i420 to YCbCr */
    v_buffer[0].width = params->width;
    v_buffer[0].stride = params->width;
    v_buffer[0].height = params->height;

    v_buffer[1].width = (v_buffer[0].width >> 1);
    v_buffer[1].height = (v_buffer[0].height >> 1);
    v_buffer[1].stride = v_buffer[1].width;

    v_buffer[2].width = v_buffer[1].width;
    v_buffer[2].height = v_buffer[1].height;
    v_buffer[2].stride = v_buffer[1].stride;

    v_buffer[0].data = v_frame;
    v_buffer[1].data = v_frame + v_buffer[0].width * v_buffer[0].height;
    v_buffer[2].data =
        v_buffer[1].data + v_buffer[0].width * v_buffer[0].height / 4;

    /* Encode 'er up */
    if (th_encode_ycbcr_in(vState->th, v_buffer) != 0) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not encode frame\n"));
        PR_Free(v_frame);
        return PR_FALSE;
    }
    if (!th_encode_packetout(vState->th, 0, &vState->op)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not read packet\n"));
        PR_Free(v_frame);
        return PR_FALSE;
    }

    ogg_stream_packetin(&vState->os, &vState->op);
    while (ogg_stream_pageout(&vState->os, &vState->og)) {
        rv = WriteData(vState->og.header, vState->og.header_len, &wr);
        rv = WriteData(vState->og.body, vState->og.body_len, &wr);
    }

    PR_Free(v_frame);
    return PR_TRUE;
}

/*
 * Encoder. Runs in a loop off the record thread until we run out of frames
 * (probably because stop was called)
 */
void
MediaRecorder::Encode()
{
    /* Alright, so we are multiplexing audio with video. We first fetch 1 frame
     * of video from the pipe, encode it and then follow up with packets of
     * audio. We want the audio and video packets to be as close to each other
     * as possible (timewise) to make playback synchronized.
     *
     * The generic formula to decide how many bytes of audio we must write per
     * frame of video will be: (SAMPLE_RATE / FPS) * AUDIO_FRAME_SIZE;
     *
     * As an example, this works out to 8820 bytes of 22050Hz audio
     * (2205 frames) per frame of video @ 10 fps.
     *
     * The really tricky part here is that most webcams will not return
     * frames at the rate we request. Movement and brightness changes will
     * often result in wildy varying fps, and we must compensate for that
     * by either dropping or duplicating frames to meet the audio stream
     * and the theora header (we commit to 15fps by default).
     *
     * TODO: Figure out if PR_Calloc will be more efficient if we call it for
     * storing more than just 1 frame at a time. For instance, we can wait
     * a second and encode 10 frames of video and 88200 bytes of audio per
     * run of the loop?
     */
    int a_read = 0;
    int v_fps = FPS_N / FPS_D;
    int a_frame_num = FRAMES_BUFFER;
    if (v_rec) {
        v_fps = vState->backend->GetFPSN() / vState->backend->GetFPSD();
        a_frame_num = params->rate/(v_fps);
    }
    int v_frame_total = vState->backend->GetFrameSize();
    int a_frame_total = a_frame_num * aState->backend->GetFrameSize();
    
    PRUint8 *v_frame;
    PRInt16 *a_frames;
    PRInt32 time_s, time_us, len;
    for (;;) {
        /* Experience suggests that audio streams are more or less consistent
         * but video frames less so. Hence, we encode a_frame_total bytes
         * of audio first - which corresponds to exactly 1 frame of video
         * which may or may not have arrived yet.
         */
        if (a_rec) {
            while (a_read < a_frame_total) {
                if (!(a_frames = GetAudioPacket(&len, &time_s, &time_us))) {
                    return;
                } else {
                    /* EncodeAudio frees a_frames */
                    if (EncodeAudio(a_frames, len) == PR_FALSE) {
                        return;
                    }
                }
                a_read += len;
            }
            a_read = 0;
        }
        
        if (v_rec) {
            if (!(v_frame = GetVideoPacket(&len, &time_s, &time_us))) {
                return;
            } else {
                /* EncodeVideo frees v_frame */
                if (EncodeVideo(v_frame, v_frame_total) == PR_FALSE) {
                    return;
                }
            }
        }

        /* We keep looping until we reach EOF on either pipe */
    }
}


/*
 * === Class methods begin now! ===
 */
nsresult
MediaRecorder::Init()
{
    a_rec = PR_FALSE;
    v_rec = PR_FALSE;

    log = PR_NewLogModule("MediaRecorder");

    pipeStream = nsnull;
    params = (Properties *)PR_Calloc(1, sizeof(Properties));
    aState = (Audio *)PR_Calloc(1, sizeof(Audio));
    vState = (Video *)PR_Calloc(1, sizeof(Video));
    return NS_OK;
}

MediaRecorder::~MediaRecorder()
{
    PR_Free(params);
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
    int i;
    nsresult rv;
    PRUint32 wr;
    ogg_uint32_t keyframe;

    if (ogg_stream_init(&vState->os, rand())) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed ogg_stream_init\n"));
        return NS_ERROR_FAILURE;
    }

    th_info_init(&vState->ti);

    /* Must be multiples of 16 */
    vState->ti.frame_width = (params->width + 15) & ~0xF;
    vState->ti.frame_height = (params->height + 15) & ~0xF;
    vState->ti.pic_width = params->width;
    vState->ti.pic_height = params->height;
    vState->ti.pic_x = (vState->ti.frame_width - params->width) >> 1 & ~1;
    vState->ti.pic_y = (vState->ti.frame_height - params->height) >> 1 & ~1;
    vState->ti.fps_numerator = params->fps_n;
    vState->ti.fps_denominator = params->fps_d;

    /* Are these the right values? */
    keyframe = 64 - 1;
    for (i = 0; keyframe; i++)
        keyframe >>= 1;
    vState->ti.quality = (int)(params->qual * 100);
    vState->ti.colorspace = TH_CS_ITU_REC_470M;
    vState->ti.pixel_fmt = TH_PF_420;
    vState->ti.keyframe_granule_shift = i;

    vState->th = th_encode_alloc(&vState->ti);
    th_info_clear(&vState->ti);

    /* Header init */
    th_comment_init(&vState->tc);
    th_comment_add_tag(&vState->tc, (char *)"ENCODER", (char *)"rainbow");
    if (th_encode_flushheader(
        vState->th, &vState->tc, &vState->op) <= 0) {
        PR_LOG(log, PR_LOG_NOTICE, ("Internal Theora library error\n"));
        return NS_ERROR_FAILURE;
    }
    th_comment_clear(&vState->tc);

    ogg_stream_packetin(&vState->os, &vState->op);
    if (ogg_stream_pageout(&vState->os, &vState->og) != 1) {
        PR_LOG(log, PR_LOG_NOTICE, ("Internal Ogg library error\n"));
        return NS_ERROR_FAILURE;
    }

    rv = WriteData(vState->og.header, vState->og.header_len, &wr);
    rv = WriteData(vState->og.body, vState->og.body_len, &wr);

    return NS_OK;
}

/*
 * Theora stream headers
 */
nsresult
MediaRecorder::SetupTheoraHeaders()
{
    int ret;
    nsresult rv;
    PRUint32 wr;

    /* Create rest of the theora headers */
    for (;;) {
        ret = th_encode_flushheader(
            vState->th, &vState->tc, &vState->op
        );
        if (ret < 0){
            PR_LOG(log, PR_LOG_NOTICE, ("Internal Theora library error\n"));
            return NS_ERROR_FAILURE;
        } else if (!ret) break;
        ogg_stream_packetin(&vState->os, &vState->op);
    }
    /* Flush the rest of theora headers. */
    for (;;) {
        ret = ogg_stream_flush(&vState->os, &vState->og);
        if (ret < 0){
            PR_LOG(log, PR_LOG_NOTICE, ("Internal Ogg library error\n"));
            return NS_ERROR_FAILURE;
        }
        if (ret == 0) break;
        rv = WriteData(vState->og.header, vState->og.header_len, &wr);
        rv = WriteData(vState->og.body, vState->og.body_len, &wr);
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
    nsresult rv;
    PRUint32 wr;

    if (ogg_stream_init(&aState->os, rand())) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed ogg_stream_init\n"));
        return NS_ERROR_FAILURE;
    }

    vorbis_info_init(&aState->vi);
    ret = vorbis_encode_init_vbr(
        &aState->vi, params->chan, params->rate, (float)params->qual
    );
    if (ret) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vorbis_encode_init\n"));
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
            PR_LOG(log, PR_LOG_NOTICE, ("Internal Ogg library error\n"));
            return NS_ERROR_FAILURE;
        }

        rv = WriteData(aState->og.header, aState->og.header_len, &wr);
        rv = WriteData(aState->og.body, aState->og.body_len, &wr);
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
    nsresult rv;
    PRUint32 wr;

    while ((ret = ogg_stream_flush(&aState->os, &aState->og)) != 0) {
        rv = WriteData(aState->og.header, aState->og.header_len, &wr);
        rv = WriteData(aState->og.body, aState->og.body_len, &wr);
    }

    return NS_OK;
}

/*
 * Make a pipe (since NS_NewPipe2 isn't exposed by XPCOM)
 */
nsresult
MediaRecorder::MakePipe(nsIAsyncInputStream **in,
            nsIAsyncOutputStream **out)
{
    nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
    if (!pipe)
        return NS_ERROR_OUT_OF_MEMORY;

    /* Video frame size > audio frame so we optimize for that */
    int size = params->width * params->height * 4;
    nsresult rv = pipe->Init(
        PR_FALSE, PR_FALSE,
        size, FRAMES_BUFFER, nsnull
    );
    if (NS_FAILED(rv))
        return rv;

    pipe->GetInputStream(in);
    pipe->GetOutputStream(out);
    return NS_OK;
}

/*
 * Parse and set properties
 */
void
MediaRecorder::ParseProperties(nsIPropertyBag2 *prop)
{
    nsresult rv;

    rv = prop->GetPropertyAsBool(NS_LITERAL_STRING("audio"), &params->audio);
    if (NS_FAILED(rv)) params->audio = PR_TRUE;
    rv = prop->GetPropertyAsBool(NS_LITERAL_STRING("video"), &params->video);
    if (NS_FAILED(rv)) params->video = PR_TRUE;
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("width"), &params->width);
    if (NS_FAILED(rv)) params->width = WIDTH;
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("height"), &params->height);
    if (NS_FAILED(rv)) params->height = HEIGHT;
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("channels"), &params->chan);
    if (NS_FAILED(rv)) params->chan = NUM_CHANNELS;
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("rate"), &params->rate);
    if (NS_FAILED(rv)) params->rate = SAMPLE_RATE;
    rv = prop->GetPropertyAsDouble(NS_LITERAL_STRING("quality"), &params->qual);
    if (NS_FAILED(rv)) params->qual = (double)SAMPLE_QUALITY;
    rv = prop->GetPropertyAsBool(NS_LITERAL_STRING("source"), &params->canvas);
    if (NS_FAILED(rv)) params->source = PR_FALSE;
}

/*
 * Start recording to file
 */
NS_IMETHODIMP
MediaRecorder::RecordToFile(
    nsIPropertyBag2 *prop,
    nsIDOMCanvasRenderingContext2D *ctx,
    nsILocalFile *file,
    nsIMediaStateObserver *obs
)
{
    nsresult rv;
    ParseProperties(prop);
    
    canvas = ctx;
    observer = obs;
    
    /* Get a file stream from the local file */
    nsCOMPtr<nsIFileOutputStream> stream(
        do_CreateInstance("@mozilla.org/network/file-output-stream;1")
    );
    pipeStream = do_QueryInterface(stream, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = stream->Init(file, -1, -1, 0);
    if (NS_FAILED(rv)) return rv;

    thread = PR_CreateThread(
        PR_SYSTEM_THREAD,
        MediaRecorder::Record, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );
    
    return NS_OK;
}

/*
 * Start recording (called in a thread)
 */
void
MediaRecorder::Record(void *data)
{   
    nsresult rv;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);
    Properties *params = mr->params;
    
    if (mr->a_rec || mr->v_rec) {
        NS_DispatchToMainThread(new MediaCallback(
            mr->observer, "error", "recording already in progress"
        ));
        return;
    }

    /* Setup backends */
    #ifdef RAINBOW_Mac
    mr->aState->backend = new AudioSourceMac(params->chan, params->rate);
    mr->vState->backend = new VideoSourceMac(params->width, params->height);
    #endif
    #ifdef RAINBOW_Win
    mr->aState->backend = new AudioSourceWin(params->chan, params->rate);
    mr->vState->backend = new VideoSourceWin(params->width, params->height);
    #endif
    #ifdef RAINBOW_Nix
    mr->aState->backend = new AudioSourceNix(params->chan, params->rate);
    mr->vState->backend = new VideoSourceNix(params->width, params->height);
    #endif
 
    /* Is the given canvas source or destination? */
    if (params->canvas) {
        mr->vState->backend = new VideoSourceCanvas(params->width, params->height);
    }
       
    /* Update parameters. What we asked for were just hints,
     * may not end up the same */
    params->fps_n = mr->vState->backend->GetFPSN();
    params->fps_d = mr->vState->backend->GetFPSD();
    params->rate = mr->aState->backend->GetRate();
    params->chan = mr->aState->backend->GetChannels();

    /* FIXME: device detection TBD */
    if (params->audio && (mr->aState == nsnull)) {
        NS_DispatchToMainThread(new MediaCallback(
            mr->observer, "error", "audio requested but no devices found"
        ));
        return;
    }
    if (params->video && (mr->vState == nsnull)) {
        NS_DispatchToMainThread(new MediaCallback(
            mr->observer, "error", "video requested but no devices found"
        ));
        return;
    }

    /* Get ready for video! */
    if (params->video) {
        mr->SetupTheoraBOS();
        rv = mr->MakePipe(
            getter_AddRefs(mr->vState->vPipeIn),
            getter_AddRefs(mr->vState->vPipeOut)
        );
        if (NS_FAILED(rv)) {
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "internal: could not create video pipe"
            ));
            return;
        }
    }

    /* Get ready for audio! */
    if (params->audio) {
        mr->SetupVorbisBOS();
        rv = mr->MakePipe(
            getter_AddRefs(mr->aState->aPipeIn),
            getter_AddRefs(mr->aState->aPipeOut)
        );
        if (NS_FAILED(rv)) {
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "internal: could not create audio pipe"
            ));
            return;
        }
    }

    /* Let's DO this. */
    if (params->video) {
        mr->SetupTheoraHeaders();
        rv = mr->vState->backend->Start(mr->vState->vPipeOut, mr->canvas);
        if (NS_FAILED(rv)) {
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "internal: could not start video recording"
            ));
            return;
        }
        mr->v_rec = PR_TRUE;
        mr->v_stp = PR_FALSE;
    }
    if (params->audio) {
        mr->SetupVorbisHeaders();
        rv = mr->aState->backend->Start(mr->aState->aPipeOut);
        if (NS_FAILED(rv)) {
            /* FIXME: Stop and clean up video! */
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "internal: could not start audio recording"
            ));
            return;
        }
        mr->a_rec = PR_TRUE;
        mr->a_stp = PR_FALSE;
    }

    /* Start off encoder after notifying observer */
    NS_DispatchToMainThread(new MediaCallback(
        mr->observer, "started", ""
    ));
    mr->Encode();
    return;
}

/*
 * Stop recording
 */
NS_IMETHODIMP
MediaRecorder::Stop()
{
    if (!a_rec && !v_rec) {
        NS_DispatchToMainThread(new MediaCallback(
            observer, "error", "no recording in progress"
        ));
        return NS_ERROR_FAILURE;
    }

    /* Return quickly and actually stop in a thread, notifying caller via
     * the 'observer' */
    PR_CreateThread(
        PR_SYSTEM_THREAD,
        MediaRecorder::StopRecord, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );
    
    return NS_OK;
}

void
MediaRecorder::StopRecord(void *data)
{
    nsresult rv;
    PRUint32 wr;
    MediaRecorder *mr = static_cast<MediaRecorder*>(data);
    
    if (mr->v_rec) {
        rv = mr->vState->backend->Stop();
        if (NS_FAILED(rv)) {
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "could not stop video recording"
            ));
            return;
        }
    }

    if (mr->a_rec) {
        rv = mr->aState->backend->Stop();
        if (NS_FAILED(rv)) {
            NS_DispatchToMainThread(new MediaCallback(
                mr->observer, "error", "could not stop audio recording"
            ));
            return;
        }
    }

    /* Wait for encoder to finish */
    if (mr->v_rec) {
        mr->v_stp = PR_TRUE;
        mr->vState->vPipeOut->Close();
    }
    if (mr->a_rec) {
        mr->a_stp = PR_TRUE;
        mr->aState->aPipeOut->Close();
    }

    PR_JoinThread(mr->thread);

    if (mr->v_rec) {
        mr->vState->vPipeIn->Close();
        th_encode_free(mr->vState->th);

        /* Video trailer */
        if (ogg_stream_flush(&mr->vState->os, &mr->vState->og)) {
            rv = mr->WriteData(
                mr->vState->og.header, mr->vState->og.header_len, &wr
            );
            rv = mr->WriteData(
                mr->vState->og.body, mr->vState->og.body_len, &wr
            );
        }

        ogg_stream_clear(&mr->vState->os);
        mr->v_rec = PR_FALSE;
    }

    if (mr->a_rec) {
        mr->aState->aPipeIn->Close();

        /* Audio trailer */
        vorbis_analysis_wrote(&mr->aState->vd, 0);
        mr->WriteAudio();

        vorbis_block_clear(&mr->aState->vb);
        vorbis_dsp_clear(&mr->aState->vd);
        vorbis_comment_clear(&mr->aState->vc);
        vorbis_info_clear(&mr->aState->vi);
        ogg_stream_clear(&mr->aState->os);
        mr->a_rec = PR_FALSE;
    }

    /* GG */
    mr->pipeStream->Close();
    NS_DispatchToMainThread(new MediaCallback(
        mr->observer, "stopped", ""
    ));
    return;
}
