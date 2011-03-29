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
 * The Original Code is Rainbow.
 *
 * The Initial Developer of the Original Code is Mozilla Labs.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "VideoSourceCanvas.h"

VideoSourceCanvas::VideoSourceCanvas(int w, int h)
    : VideoSource(w, h)
{
    /* Nothing much to do here. We sample at a constant 30fps */
    fps_n = 30;
    fps_d = 1;
    g2g = PR_TRUE;
    recording = PR_FALSE;
}

VideoSourceCanvas::~VideoSourceCanvas()
{
}

nsresult
VideoSourceCanvas::Start(nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    vCanvas = ctx;
    return NS_OK;
}

nsresult
VideoSourceCanvas::StartRecording(nsIOutputStream *pipe)
{
    if (!g2g)
        return NS_ERROR_FAILURE;
        
    /* Start a thread to sample canvas */
    output = pipe;
    recording = PR_TRUE;
    sampler = PR_CreateThread(
        PR_SYSTEM_THREAD, VideoSourceCanvas::Grabber, this,
        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0
    );
    
    return NS_OK;
}

nsresult
VideoSourceCanvas::StopRecording()
{
    if (!g2g)
        return NS_ERROR_FAILURE;
    recording = PR_FALSE;
    PR_JoinThread(sampler);
    return NS_OK;
}

nsresult
VideoSourceCanvas::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

void
VideoSourceCanvas::Grabber(void *data)
{
    nsresult rv;
    PRUint32 wr;
    VideoSourceCanvas *vs = static_cast<VideoSourceCanvas*>(data);
    
    int isize = vs->GetFrameSize();
    int fsize = vs->width * vs->height * 4;
    char *i420 = (char *)PR_Calloc(isize, 1);
    PRUint8 *rgb32 = (PRUint8 *)PR_Calloc(fsize, 1);

    PRIntervalTime ticks = PR_TicksPerSecond() / 30;
    while (vs->recording) {
        PR_Sleep(ticks);
        rv = vs->vCanvas->GetImageData_explicit(
            0, 0, vs->width, vs->height, rgb32, fsize
        );
        if (NS_FAILED(rv)) continue;

        RGB32toI420(vs->width, vs->height, (const char *)rgb32, i420);
        rv = vs->output->Write((const char *)i420, isize, &wr);
    }

    PR_Free(i420);
    PR_Free(rgb32);
    return;
}
