/*
 * Video source implementation for Mac.
 * We're using libvidcap but we should switch to Quicktime
 * for 64-bit compliance.
 */

#include "VideoSource.h"

#include <prmem.h>
#include <vidcap/vidcap.h>
#include <vidcap/converters.h>

class VideoSourceMac : public VideoSource {
public:
    VideoSourceMac(int n, int d, int w, int h);
    ~VideoSourceMac();

    nsresult Stop();
    nsresult Start(nsIOutputStream *pipe);

protected:
    vidcap_sapi *sapi;
    vidcap_src *source;
    vidcap_state *state;
    nsIOutputStream *output;
    struct vidcap_src_info *sources;

    static int Callback(
        vidcap_src *src, void *data, struct vidcap_capture_info *video
    );
        
};

