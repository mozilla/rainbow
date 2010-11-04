#include <nsIOutputStream.h>

/*
 * Interface for video sources to implement
 * TODO: Figure out if and how to do device selection
 */
class VideoSource {
public:
    virtual nsresult Stop();
    virtual nsresult Start(nsIOutputStream *pipe);
    virtual void SetOptions(int fps_n, int fps_d, int width, int height);
};

