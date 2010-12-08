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

#include <stdio.h>
#include "VideoSourceKinect.h"

/* Unfrotunately, the libfreenect callback does not give us the
 * opportunity to pass a custom void* so we are reduced to using a global!
 */
PRUint16 *glb_gamma;
nsIDOMCanvasRenderingContext2D *glb_canvas;

VideoSourceKinect::VideoSourceKinect(int w, int h)
    : VideoSource(w, h)
{
    int i, n;
    g2g = PR_FALSE;
    running = PR_FALSE;

    /* Only support 640 * 480 */
    if (w != 640 || h != 480) return;
    
    glb_gamma = (PRUint16 *)PR_Calloc(1024, 2);
    for (i = 0; i < 2048; i++) {
        float v = i / 2048.0;
        v = powf(v, 3) * 6;
        glb_gamma[i] = (PRUint16)(v * 6 * 256);
    }

    if (freenect_init(&f_ctx, NULL) < 0) return;

#ifdef DEBUG
    freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
#endif

    n = freenect_num_devices(f_ctx);
    if (n < 1) return;

    if (freenect_open_device(f_ctx, &f_dev, 0) < 0) return;
    
    /* Default 30 fps */
    fps_n = 30;
    fps_d = 1;
    
    g2g = PR_TRUE;
}

VideoSourceKinect::~VideoSourceKinect()
{
    freenect_close_device(f_dev);
    freenect_shutdown(f_ctx);
    PR_Free(glb_gamma);
}

nsresult
VideoSourceKinect::Start(
    nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;
    
    glb_canvas = ctx;
    
    freenect_set_tilt_degs(f_dev, 0);
    freenect_set_led(f_dev, LED_RED);
    freenect_set_depth_callback(f_dev, DepthCallback);
    freenect_set_depth_format(f_dev, FREENECT_DEPTH_11BIT);
    
    necthr = PR_CreateThread(
        PR_SYSTEM_THREAD,
        VideoSourceKinect::FreenectThread, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );

    output = pipe;
    return NS_OK;
}

void
VideoSourceKinect::FreenectThread(void *data)
{
    VideoSourceKinect *kr = static_cast<VideoSourceKinect*>(data);

    kr->running = PR_TRUE;
    freenect_start_depth(kr->f_dev);
    PRIntervalTime wait = PR_TicksPerSecond() / (kr->fps_n / kr->fps_d);

    while (kr->running && freenect_process_events(kr->f_ctx) >= 0) {
        // Chance to get acceloremeter data as well as change tilt/LED
        // Let's sleep for fps/msec to prevent starvation
        PR_Sleep(wait);
    }
    
    freenect_stop_depth(kr->f_dev);
}
    
nsresult
VideoSourceKinect::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    running = PR_FALSE;
    PR_JoinThread(necthr);

    return NS_OK;
}

void
VideoSourceKinect::DepthCallback(freenect_device *dev, void *depth, uint32_t time)
{
    fprintf(stderr, "called\n");

    int i, fsize = 640 * 480 * 4;
    PRUint16 *dp = (PRUint16 *)depth;
    nsAutoArrayPtr<PRUint8> rgb32(new PRUint8[fsize]);
    PRUint8 *buffer = rgb32.get();

    /* Convert 11bit depth values to rgb32 */
    for (i = 0; i < FREENECT_FRAME_PIX; i++) {
        int pval = glb_gamma[dp[i]];
        int lb = pval & 0xff;

        switch (pval >> 8) {
			case 0:
				buffer[4*i+0] = 255;
				buffer[4*i+1] = 255 - lb;
				buffer[4*i+2] = 255 - lb;
				break;
			case 1:
				buffer[4*i+0] = 255;
				buffer[4*i+1] = lb;
				buffer[4*i+2] = 0;
				break;
			case 2:
				buffer[4*i+0] = 255 - lb;
				buffer[4*i+1] = 255;
				buffer[4*i+2] = 0;
				break;
			case 3:
				buffer[4*i+0] = 0;
				buffer[4*i+1] = 255;
				buffer[4*i+2] = lb;
				break;
			case 4:
				buffer[4*i+0] = 0;
				buffer[4*i+1] = 255 - lb;
				buffer[4*i+2] = 255;
				break;
			case 5:
				buffer[4*i+0] = 0;
				buffer[4*i+1] = 0;
				buffer[4*i+2] = 255 - lb;
				break;
			default:
				buffer[4*i+0] = 0;
				buffer[4*i+1] = 0;
				buffer[4*i+2] = 0;
				break;
		}
    }

    if (glb_canvas) {
        nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
            glb_canvas, 640, 480, rgb32, fsize
        );
        NS_DispatchToMainThread(render);
        fprintf(stderr, "printed\n");
    }
}

