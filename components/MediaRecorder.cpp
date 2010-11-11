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
 *   Brian Coleman <brianfcoleman@gmail.com>
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
 * Convert RGB32 to i420. NOTE: size of dest must be >= width * height * 3 / 2
 * Based on formulas found at http://en.wikipedia.org/wiki/YUV  (libvidcap)
 */
static int
RGB32toI420(int width, int height, const char *src, char *dst)
{
    int i, j;
    unsigned char *dst_y_even;
    unsigned char *dst_y_odd;
    unsigned char *dst_u;
    unsigned char *dst_v;
    const unsigned char *src_even;
    const unsigned char *src_odd;

    src_even = (const unsigned char *)src;
    src_odd = src_even + width * 4;

    dst_y_even = (unsigned char *)dst;
    dst_y_odd = dst_y_even + width;
    dst_u = dst_y_even + width * height;
    dst_v = dst_u + ((width * height) >> 2);

    for (i = 0; i < height / 2; ++i) {
        for (j = 0; j < width / 2; ++j) {
            short r, g, b;
            b = *src_even++;
            g = *src_even++;
            r = *src_even++;
            ++src_even;
            *dst_y_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

            *dst_u++ = (( r * -38 - g * 74 + b * 112 + 128 ) >> 8 ) + 128;
            *dst_v++ = (( r * 112 - g * 94 - b * 18 + 128 ) >> 8 ) + 128;

            b = *src_even++;
            g = *src_even++;
            r = *src_even++;
            ++src_even;
            *dst_y_even++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

            b = *src_odd++;
            g = *src_odd++;
            r = *src_odd++;
            ++src_odd;
            *dst_y_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;

            b = *src_odd++;
            g = *src_odd++;
            r = *src_odd++;
            ++src_odd;
            *dst_y_odd++ = (( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16;
        }

        dst_y_odd += width;
        dst_y_even += width;
        src_odd += width * 4;
        src_even += width * 4;
    }

    return 0;
}

/* Rendering on Canvas happens on the main thread as this runnable.
 * This is not very performant, we should move to rendering inside a <video>
 * so that Gecko can use hardware acceleration.
 */
class CanvasRenderer : public nsRunnable
{
public:
    CanvasRenderer(
        nsIDOMCanvasRenderingContext2D *pCtx, PRUint32 width, PRUint32 height,
        nsAutoArrayPtr<PRUint8> &pData, PRUint32 pDataSize)
        :   m_pCtx(pCtx), m_width(width), m_height(height),
            m_pData(pData), m_pDataSize(pDataSize) {}
    
    NS_IMETHOD Run() {
        return m_pCtx->PutImageData_explicit(
            0, 0, m_width, m_height, m_pData.get(), m_pDataSize
        );
    }

private:
    nsIDOMCanvasRenderingContext2D *m_pCtx;
    PRUint32 m_width;
    PRUint32 m_height;
    nsAutoArrayPtr<PRUint8> m_pData;
    PRUint32 m_pDataSize;

};


/*
 * === Here's the meat of the code. Static callbacks & encoder ===
 */

void
MediaRecorder::WriteAudio(void *data)
{
    int ret;
    nsresult rv;
    PRUint32 wr;
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
                
                rv = mr->WriteData(
                    mr, mr->aState->og.header, mr->aState->og.header_len, &wr
                );           
                rv = mr->WriteData(
                    mr, mr->aState->og.body, mr->aState->og.body_len, &wr
                );

                if (ogg_page_eos(&mr->aState->og))
                    break;
            }
        }
    }
}

nsresult
MediaRecorder::WriteData(
    void *obj, unsigned char *data, PRUint32 len, PRUint32 *wr)
{
    nsresult rv;
    PRUint32 av;
    char *decoded;
    char *encoded;
    PRBool success;
    MediaRecorder *mr = static_cast<MediaRecorder*>(obj);

    if (mr->pipeFile) {
        return mr->pipeFile->Write((const char*)data, len, wr);
    }

    if (mr->pipeSock) {
        /* We're batching websocket writes every SOCK_LEN bytes */
        mr->sockOut->Write((const char*)data, len, wr);

        if (NS_SUCCEEDED(mr->sockIn->Available(&av)) && av >= SOCK_LEN) {
            decoded = (char *)PR_Calloc(SOCK_LEN, 1);
            rv = mr->sockIn->Read(decoded, SOCK_LEN, &av);

            /* Base64 is ((srclen + 2)/3)*4 in size. We do this because
             * websockets cannot send binary data (yet) */
            encoded = PL_Base64Encode((const char*)decoded, SOCK_LEN, nsnull);
            rv = mr->pipeSock->Send(NS_ConvertUTF8toUTF16(encoded), &success);

            PR_Free(decoded);
            PR_Free(encoded);
            return rv;     
        }
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}


/*
 * Encoder, runs in a thread
 */
void
MediaRecorder::Encode(void *data)
{
    int i, j;
    nsresult rv;
    PRUint32 wr;
    PRUint32 rd = 0;

    char *a_frame;
    float **a_buffer;
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
    int a_frame_len = mr->params->rate/(mr->params->fps_n/mr->params->fps_d);
    int v_frame_size = mr->vState->backend->GetFrameSize();
    int a_frame_size = mr->aState->backend->GetFrameSize();
    int a_frame_total = a_frame_len * a_frame_size;

    for (;;) {

        if (mr->v_rec) {

        /* Make sure we get 1 full frame of video */
        nsAutoArrayPtr<PRUint8> v_frame(new PRUint8[v_frame_size]);
        do mr->vState->vPipeIn->Available(&rd);
            while ((rd < (PRUint32)v_frame_size) && !mr->v_stp);
        rv = mr->vState->vPipeIn->Read((char *)v_frame.get(), v_frame_size, &rd);

        if (rd == 0) {
            /* EOF. If there's audio we need to finish it. Goto: gasp! */
            if (mr->a_rec)
                goto audio_enc;
            else
                return;
        } else if (rd != (PRUint32)v_frame_size) {
            /* Now, we're in a pickle. HOW TO FIXME? */
            PR_LOG(mr->log, PR_LOG_NOTICE,
                ("only read %u of %d from video pipe\n", rd, v_frame_size));
            PR_Free(v_frame);
            return;
        }

        /* Convert RGB32 to i420 */
        nsAutoArrayPtr<PRUint8> i420(
            new PRUint8[mr->params->width * mr->params->height * 3 / 2]);

        RGB32toI420(mr->params->width, mr->params->height,
            (const char *)v_frame.get(), (char *)i420.get());

        /* Convert i420 to YCbCr */
        v_buffer[0].width = mr->params->width;
        v_buffer[0].stride = mr->params->width;
        v_buffer[0].height = mr->params->height;

        v_buffer[1].width = (v_buffer[0].width >> 1);
        v_buffer[1].height = (v_buffer[0].height >> 1);
        v_buffer[1].stride = v_buffer[1].width;

        v_buffer[2].width = v_buffer[1].width;
        v_buffer[2].height = v_buffer[1].height;
        v_buffer[2].stride = v_buffer[1].stride;

        v_buffer[0].data = i420.get();
        v_buffer[1].data = i420.get() + v_buffer[0].width * v_buffer[0].height;
        v_buffer[2].data =
            v_buffer[1].data + v_buffer[0].width * v_buffer[0].height / 4;

        /* Encode 'er up */
        if (th_encode_ycbcr_in(mr->vState->th, v_buffer) != 0) {
            PR_LOG(mr->log, PR_LOG_NOTICE, ("Could not encode frame\n"));
            return;
        }
        if (!th_encode_packetout(mr->vState->th, 0, &mr->vState->op)) {
            PR_LOG(mr->log, PR_LOG_NOTICE, ("Could not read packet\n"));
            return;
        }
        
        /* Write to canvas, if needed */
        if (mr->vState->vCanvas) {
            /* OSes give us BGRA but we want RGBA */
            PRUint8 tmp = 0;
            PRUint8 *pData = v_frame.get();
            for (int j = 0; j < v_frame_size; j += 4) {
                tmp = pData[j+2];
                pData[j+2] = pData[j];
                pData[j] = tmp;
            }
            
            nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
                mr->vState->vCanvas,
                mr->params->width, mr->params->height,
                v_frame, v_frame_size
            );
            rv = NS_DispatchToMainThread(render);
        }

        ogg_stream_packetin(&mr->vState->os, &mr->vState->op);
        while (ogg_stream_pageout(&mr->vState->os, &mr->vState->og)) {
            rv = mr->WriteData(
                mr, mr->vState->og.header, mr->vState->og.header_len, &wr
            );
            rv = mr->WriteData(
                mr, mr->vState->og.body, mr->vState->og.body_len, &wr
            );
        }

        }

audio_enc:
    
        if (mr->a_rec) {
             
        /* Make sure we get enough frames in unless we're at the end */
        a_frame = (char *) PR_Calloc(1, a_frame_total);

        do mr->aState->aPipeIn->Available(&rd);
            while ((rd < (PRUint32)a_frame_total) && !mr->a_stp);
        rv = mr->aState->aPipeIn->Read((char *)a_frame, a_frame_total, &rd);

        if (rd == 0) {
            /* EOF. We're done. */
            PR_Free(a_frame);
            return;
        } else if (rd != (PRUint32)a_frame_total) {
            /* Hmm. I sure hope this is the end of the recording. */
            PR_Free(a_frame);
            if (!mr->a_stp) {
                PR_LOG(mr->log, PR_LOG_NOTICE,
                    ("only read %u of %d from audio pipe\n", rd, a_frame_total));
            }
            return;
        }
          
        /* Uninterleave samples. Alternatively, have portaudio do this? */
        a_buffer = vorbis_analysis_buffer(&mr->aState->vd, a_frame_total);
        for (i = 0; i < a_frame_len; i++){
            for (j = 0; j < (int)mr->params->chan; j++) {
                a_buffer[j][i] =
                    (float)((a_frame[i*a_frame_size+((j*2)+1)]<<8) |
                        ((0x00ff&(int)a_frame[i*a_frame_size+(j*2)]))) /
                            32768.f;
            }
        }
        
        /* Tell libvorbis to do its thing */
        vorbis_analysis_wrote(&mr->aState->vd, a_frame_len);
        MediaRecorder::WriteAudio(data);
        PR_Free(a_frame);

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

    pipeSock = nsnull;
    pipeFile = nsnull;

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
    nsresult rv;
    PRUint32 wr;

    if (ogg_stream_init(&vState->os, rand())) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed ogg_stream_init\n"));
        return NS_ERROR_FAILURE;
    }
    
    th_info_init(&vState->ti);
    /* Must be multiples of 16 */
    vState->ti.frame_width = ((params->width + 15) >> 4) << 4;
    vState->ti.frame_height = ((params->height + 15) >> 4) << 4;
    vState->ti.pic_width = params->width;
    vState->ti.pic_height = params->height;
    vState->ti.pic_x = 0;
    vState->ti.pic_y = 0;
    
    /* Too fast? Why? */
    vState->ti.fps_numerator = params->fps_n;
    vState->ti.fps_denominator = params->fps_d;
    vState->ti.aspect_numerator = 0;
    vState->ti.aspect_denominator = 0;
    vState->ti.target_bitrate = 0;

    /* Are these the right values? */ 
    vState->ti.quality = (int)(params->qual * 100);
    vState->ti.colorspace = TH_CS_UNSPECIFIED;
    vState->ti.pixel_fmt = TH_PF_420;

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
    
    rv = WriteData(
        this, vState->og.header, vState->og.header_len, &wr
    );
    rv = WriteData(
        this, vState->og.body, vState->og.body_len, &wr
    );

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
        rv = WriteData(
            this, vState->og.header, vState->og.header_len, &wr
        );
        rv = WriteData(
            this, vState->og.body, vState->og.body_len, &wr
        );
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
        
        rv = WriteData(
            this, aState->og.header, aState->og.header_len, &wr
        );
        rv = WriteData(
            this, aState->og.body, aState->og.body_len, &wr
        );
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
        rv = WriteData(
            this, aState->og.header, aState->og.header_len, &wr
        );
        rv = WriteData(
            this, aState->og.body, aState->og.body_len, &wr
        );
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
 * Start recording to file
 */
NS_IMETHODIMP
MediaRecorder::RecordToFile(
    nsIPropertyBag2 *prop,
    nsIDOMCanvasRenderingContext2D *ctx,
    nsILocalFile *file
)
{
    nsresult rv;
    ParseProperties(prop);

    /* Get a file stream from the local file */
    nsCOMPtr<nsIFileOutputStream> stream(
        do_CreateInstance("@mozilla.org/network/file-output-stream;1")
    );
    
    pipeFile = do_QueryInterface(stream, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = stream->Init(file, -1, -1, 0);
    if (NS_FAILED(rv)) return rv;

    return Record(ctx);
}

/*
 * Start recording to web socket
 */
NS_IMETHODIMP
MediaRecorder::RecordToSocket(
    nsIPropertyBag2 *prop,
    nsIDOMCanvasRenderingContext2D *ctx,
    nsIWebSocket *sock
)
{
    pipeSock = sock;    
    ParseProperties(prop);
    MakePipe(getter_AddRefs(sockIn), getter_AddRefs(sockOut));

    return Record(ctx);
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
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("fps_n"), &params->fps_n);
    if (NS_FAILED(rv)) params->fps_n = FPS_N;
    rv = prop->GetPropertyAsUint32(NS_LITERAL_STRING("fps_d"), &params->fps_d);
    if (NS_FAILED(rv)) params->fps_d = FPS_D;
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
}

/*
 * Actual record method
 */
nsresult
MediaRecorder::Record(nsIDOMCanvasRenderingContext2D *ctx)
{
    nsresult rv; 
    if (a_rec || v_rec) {
        PR_LOG(log, PR_LOG_NOTICE, ("Recording in progress\n"));
        return NS_ERROR_FAILURE;
    }

    /* Setup video backend */
    #ifdef RAINBOW_Mac
    vState->backend = new VideoSourceMac(
        params->fps_n, params->fps_d, params->width, params->height
    );
    #endif
    #ifdef RAINBOW_Win
    vState->backend = new VideoSourceWin(
        params->fps_n, params->fps_d, params->width, params->height
    );
    #endif

    /* Audio backend same for all platforms */
    aState->backend = new AudioSourcePortaudio(
        params->chan, params->rate, (float)params->qual
    );

    /* FIXME: device detection TBD */
    if (params->audio && (aState == nsnull)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Audio requested but no devices found\n"));
        return NS_ERROR_FAILURE;
    }
    if (params->video && (vState == nsnull)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Video requested but no devices found\n"));
        return NS_ERROR_FAILURE;
    }

    /* Get ready for video! */
    if (params->video) {
        if (ctx) {
            vState->vCanvas = ctx;
        }
        SetupTheoraBOS();
        rv = MakePipe(
            getter_AddRefs(vState->vPipeIn),
            getter_AddRefs(vState->vPipeOut)
        );
        if (NS_FAILED(rv)) return rv;
    }

    /* Get ready for audio! */
    if (params->audio) {
        SetupVorbisBOS();
        rv = MakePipe(
            getter_AddRefs(aState->aPipeIn),
            getter_AddRefs(aState->aPipeOut)
        );
        if (NS_FAILED(rv)) return rv;
    }

    /* Let's DO this. */
    if (params->video) {
        SetupTheoraHeaders();
        rv = vState->backend->Start(vState->vPipeOut);
        if (NS_FAILED(rv)) return rv;
        v_rec = PR_TRUE;
        v_stp = PR_FALSE;
    }
    if (params->audio) {
        SetupVorbisHeaders();
        rv = aState->backend->Start(aState->aPipeOut);
        if (NS_FAILED(rv)) return rv;
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
    nsresult rv;
    PRUint32 wr;

    if (!a_rec && !v_rec) {
        PR_LOG(log, PR_LOG_NOTICE, ("No recording in progress\n"));
        return NS_ERROR_FAILURE;    
    }

    if (v_rec) {
        rv = vState->backend->Stop();
        if (NS_FAILED(rv)) return rv;
    }
    
    if (a_rec) {
        rv = aState->backend->Stop();
        if (NS_FAILED(rv)) return rv;
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
            rv = WriteData(
                this, vState->og.header, vState->og.header_len, &wr
            );
            rv = WriteData(
                this, vState->og.body, vState->og.body_len, &wr
            );
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
    if (pipeFile) {
        pipeFile->Close();
        pipeFile = nsnull;
    }
    if (pipeSock) {
        pipeSock = nsnull;
    }

    return NS_OK;
}

