/*
 * Common VideoSource implementation
 */
#include "VideoSource.h"

VideoSource::VideoSource(int n, int d, int w, int h)
{
    fps_n = n;
    fps_d = d;
    width = w;
    height = h;

    /* Setup logger */
    log = PR_NewLogModule("VideoSource");
}

int
VideoSource::GetFrameSize()
{
    /* RGB32 is 4 bytes per pixel */
    return width * height * 4;
}

