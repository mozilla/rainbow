#include "VideoSourceMac.h"

VideoSourceMac::VideoSourceMac(int n, int d, int w, int h)
    : VideoSource(n, d, w, h)
{
    /* Setup video devices */
    int nd = 0;
    struct vidcap_sapi_info sapi_info;
    if (!(state = vidcap_initialize())) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not initialize vidcap\n"));
        return;
    }
    if (!(sapi = vidcap_sapi_acquire(state, 0))) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed to acquire default sapi\n"));
        return;
    }
    if (vidcap_sapi_info_get(sapi, &sapi_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed to get default sapi info\n"));
        return;
    }

    nd = vidcap_src_list_update(sapi);
    if (nd < 0) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_list_update\n"));
        return;
    } else if (nd == 0) {
        /* No video capture device available */
        PR_LOG(log, PR_LOG_DEBUG, ("No video devices were found!\n"));
        return;
    } else {
        if (!(sources = (struct vidcap_src_info *)
            PR_Calloc(nd, sizeof(struct vidcap_src_info)))) {
            return;
        }
        if (vidcap_src_list_get(sapi, nd, sources)) {
            PR_Free(sources);
            PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_list_get\n"));
            return;
        }
    }
}

VideoSourceMac::~VideoSourceMac()
{
    vidcap_sapi_release(sapi);
    vidcap_destroy(state);
    PR_Free(sources);
}

nsresult
VideoSourceMac::Start(nsIOutputStream *pipe)
{
    struct vidcap_fmt_info fmt_info;
    fmt_info.width = width;
    fmt_info.height = height;
    fmt_info.fps_numerator = fps_n;
    fmt_info.fps_denominator = fps_d;

    /* FIXME: We're requesting i420 but converting to RGB32 because libvidcap
     * returns side-scrolling, incorrect video when we request RGB32. We are
     * converting back to i420 (so we can get ycbcr) in the mutliplexer so this
     * is duplication but most cameras return RGB32 and the problem will go
     * go away when we switch to Quicktime
     */
    fmt_info.fourcc = VIDCAP_FOURCC_I420;
    
    if (!(source = vidcap_src_acquire(sapi, &sources[0]))) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_acquire\n"));
        return NS_ERROR_FAILURE;
    }
       
    if (vidcap_format_bind(source, &fmt_info)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_format_bind\n"));
        return NS_ERROR_FAILURE;
    }

    if (vidcap_src_capture_start(source, VideoSourceMac::Callback, this)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_capture_start\n"));
        return NS_ERROR_FAILURE;
    }

    output = pipe;
    return NS_OK;
}

nsresult
VideoSourceMac::Stop()
{
    if (vidcap_src_capture_stop(source)) {
        PR_LOG(log, PR_LOG_NOTICE, ("Failed vidcap_src_capture_stop\n"));
        return NS_ERROR_FAILURE;
    }

    vidcap_src_release(source);
    return NS_OK;
}

int
VideoSourceMac::Callback(
    vidcap_src *src, void *data, struct vidcap_capture_info *video)
{
    /*
     * See note above to see when this ugliness will go away.
     */
    VideoSourceMac *vsm = static_cast<VideoSourceMac*>(data);

    nsresult rv;
    PRUint32 wr;
    char *rgbptr;
    char *i420ptr;
    int rgbfsize = vsm->GetFrameSize();
    int i420fsize = vsm->width * vsm->height * 3 / 2;
    int frames = video->video_data_size  / i420fsize;
    char *rgb = (char *)PR_Calloc(rgbfsize, frames);

    for (int i = 0; i < frames; i++) {
        rgbptr = rgb + (i * rgbfsize);
        i420ptr = (char *)video->video_data + (i * i420fsize);
        vidcap_i420_to_rgb32(vsm->width, vsm->height, i420ptr, rgbptr);
    }

    rv = vsm->output->Write((const char *)rgb, rgbfsize * frames, &wr);
    PR_Free(rgb);
    return 0;
}

