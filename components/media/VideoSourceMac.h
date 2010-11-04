/*
 * Video source implementation for Mac.
 * We're using libvidcap but we should switch to Quicktime
 * for 64-bit compliance.
 */

#include "VideoSource.h"
#include <prlog.h>
#include <prmem.h>
#include <vidcap/vidcap.h>
#include <vidcap/converters.h>

class VideoSourceMac : public VideoSource {
public:
    VideoSourceMac();
    ~VideoSourceMac();

    nsresult Stop();
    nsresult Start(nsIOutputStream *pipe);
    void SetOptions(int fps_n, int fps_d, int width, int height);

protected:
    PRLogModuleInfo *log;
    
    vidcap_sapi *sapi;
    vidcap_src *source;
    vidcap_state *state;
    struct vidcap_src_info *sources;

    static int Callback(
        vidcap_src *src, void *data, struct vidcap_capture_info *video
    );

private:
    int fps_n;
    int fps_d;
    int width;
    int height;
    nsIOutputStream *output;
};

