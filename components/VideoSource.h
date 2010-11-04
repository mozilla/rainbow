/*
 * Interface for video sources to implement
 * TODO: Figure out if and how to do device selection
 */
#include <prlog.h>
#include <nsIOutputStream.h>

/* Defaults */
#define FPS_N   12
#define FPS_D   1
#define WIDTH   640
#define HEIGHT  480

class VideoSource {
public:
    /* Re-use the constructor and frame size getter */
    VideoSource(int fps_n, int fps_d, int width, int height);
    int GetFrameSize();

    /* Implement these two. Write RGB32 samples to pipe */
    virtual nsresult Stop() = 0;
    virtual nsresult Start(nsIOutputStream *pipe) = 0;

protected:
    int fps_n;
    int fps_d;
    int width;
    int height;

    PRLogModuleInfo *log;

};

