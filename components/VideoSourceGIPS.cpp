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

#include "VideoSourceGIPS.h"
#define MICROSECONDS 1000000

VideoSourceGIPS::VideoSourceGIPS(int w, int h)
    : VideoSource(w, h)
{
    int error = 0;
    videoChannel = -1;

    ptrViE = webrtc::VideoEngine::Create();
    if (ptrViE == NULL) {
        fprintf(stderr, "ERROR in GIPSVideoEngine::Create\n");
        return;
    }

    error = ptrViE->SetTraceFile("GIPSViETrace.txt");
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSVideoEngine::SetTraceFile\n");
        return;
    }

    ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    if (ptrViEBase == NULL) {
        printf("ERROR in GIPSViEBase::GIPSViE_GetInterface\n");
        return;
    }
    error = ptrViEBase->Init();
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViEBase::GIPSViE_Init\n");
        return;
    }

    error = ptrViEBase->CreateChannel(videoChannel);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViEBase::GIPSViE_CreateChannel\n");
        return;
    }

    /* List available capture devices, allocate and connect. */
    ptrViECapture =
        webrtc::ViECapture::GetInterface(ptrViE);
    if (ptrViEBase == NULL) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_GetInterface\n");
        return;
    }

    if (ptrViECapture->NumberOfCaptureDevices() <= 0) {
        fprintf(stderr, "ERROR no video devices found\n");
        return;
    }

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    error = ptrViECapture->GetCaptureDevice(0, deviceName,
        KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_GetCaptureDevice\n");
        return;
    }
    fprintf(stderr, "Got device name='%s' id='%s'\n", deviceName, uniqueId);

    int numCaps = ptrViECapture->NumberOfCapabilities(
        uniqueId, KMaxUniqueIdLength
    );
    fprintf(stderr, "Got %d capabilities!\n", numCaps);

    for (int i = 0; i < numCaps; i++) {
        error = ptrViECapture->GetCaptureCapability(
            uniqueId, KMaxUniqueIdLength, i, cap
        );
        fprintf(stderr,
            "%d. w=%d, h=%d, fps=%d, d=%d, i=%d, RGB24=%d, i420=%d, ARGB=%d\n",
            i, cap.width, cap.height,
            cap.maxFPS, cap.expectedCaptureDelay, cap.interlaced,
            cap.rawType == webrtc::kVideoRGB24,
            cap.rawType == webrtc::kVideoI420,
            cap.rawType == webrtc::kVideoARGB);
    }

    // Set to third capability
    error = ptrViECapture->GetCaptureCapability(
        uniqueId, KMaxUniqueIdLength, 2, cap
    );

    gipsCaptureId = 0;
    error = ptrViECapture->AllocateCaptureDevice(uniqueId,
        KMaxUniqueIdLength, gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_AllocateCaptureDevice\n");
        return;
    }

    fps_d = 1;
    fps_n = cap.maxFPS;

    is_rec = PR_FALSE;
    g2g = PR_TRUE;
}

VideoSourceGIPS::~VideoSourceGIPS()
{
    int error = ptrViECapture->ReleaseCaptureDevice(gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_ReleaseCaptureDevice\n");
    }

    error = ptrViEBase->DeleteChannel(videoChannel);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViEBase::GIPSViE_DeleteChannel\n");
    }

    ptrViECapture->Release();
    ptrViEBase->Release();

    if (webrtc::VideoEngine::Delete(ptrViE) == false) {
        fprintf(stderr, "ERROR in GIPSVideoEngine::Delete\n");
    }
}

nsresult
VideoSourceGIPS::Start(nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    int error = 0;
    vCanvas = ctx;

    error = ptrViECapture->ConnectCaptureDevice(gipsCaptureId,
        videoChannel);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_ConnectCaptureDevice\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViECapture->StartCapture(gipsCaptureId, cap);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_StartCapture\n");
        return NS_ERROR_FAILURE;
    }

    // Setup external renderer
    ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
    if (ptrViERender == NULL) {
        fprintf(stderr, "ERROR in GIPSViERender::GIPSViE_GetInterface\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViERender->AddRenderer(gipsCaptureId, webrtc::kVideoI420, this);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViERender::GIPSViE_AddRenderer\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViERender->StartRender(gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViERender::GIPSViE_StartRender\n");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
VideoSourceGIPS::Stop()
{
    int error = 0;
    if (!g2g)
        return NS_ERROR_FAILURE;

    is_rec = PR_FALSE;

    error = ptrViERender->StopRender(gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViERender::GIPSViE_StopRender\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViERender->RemoveRenderer(gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViERender::GIPSViE_RemoveRenderer\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViECapture->StopCapture(gipsCaptureId);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_StopCapture\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSViECapture::GIPSViE_DisconnectCaptureDevice\n");
        return NS_ERROR_FAILURE;
    }

    ptrViERender->Release();
    ptrViECapture->Release();

    return NS_OK;
}

int
VideoSourceGIPS::FrameSizeChange(
    unsigned int width, unsigned int height, unsigned int numberOfStreams)
{
    // XXX: Hmm?
    fprintf(stderr, "\nGot FrameSizeChange: %d %d\n", width, height);
    return -1;
}

int
VideoSourceGIPS::DeliverFrame(unsigned char* buffer, int bufferSize)
{
    nsresult rv;
    PRUint32 wr;
    
    PRTime epoch_c = PR_Now();
    PRFloat64 epoch = (PRFloat64)(epoch_c / MICROSECONDS);
    epoch += ((PRFloat64)(epoch_c % MICROSECONDS)) / MICROSECONDS;

    if (is_rec) {
        /* Write header: timestamp + length */
        rv = output->Write(
            (const char *)&epoch, sizeof(PRFloat64), &wr
        );
        rv = output->Write(
            (const char *)&bufferSize, sizeof(PRUint32), &wr
        );
        /* Write data */
        rv = output->Write(
            (const char *)buffer, bufferSize, &wr
        );
    }
    
    /* Write preview to canvas, if needed */
    int fsize = width * height * 4;
    if (vCanvas) {
        /* Convert i420 to RGB32 to write on canvas */
        nsAutoArrayPtr<PRUint8> rgb32(new PRUint8[fsize]);
        I420toRGB32(width, height,
            (const char *)buffer, (char *)rgb32.get()
        );

        nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
            vCanvas, width, height, rgb32, fsize
        );
        rv = NS_DispatchToMainThread(render);
    }

    return 0;
}
