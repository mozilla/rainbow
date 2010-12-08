/*
 * This file was derived from the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors.
 * Copyright (c) 2010 Mozilla Foundation.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


/*
 * kinect video source implementation made possible thanks to the awesome folks
 * at openkinect.
 */
#include "VideoSource.h"

#include <math.h>
#include <prmem.h>
#include <prlock.h>
#include <libfreenect.h>

class VideoSourceKinect : public VideoSource {
public:
    VideoSourceKinect(int w, int h);
    ~VideoSourceKinect();

    nsresult Stop();
    nsresult Start(nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx);

protected:
    freenect_device *f_dev;
    freenect_context *f_ctx;
    
    PRBool running;
    PRThread *necthr;
    nsIOutputStream *output;
    
    static void FreenectThread(void *data);
    static void DepthCallback(
        freenect_device *dev, void *depth, uint32_t time
    );
    /*
    static void RGBCallback(
        freenect_device *dev, void *rgb, uint32_t time
    );
    */
};

