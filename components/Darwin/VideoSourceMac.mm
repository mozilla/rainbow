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

#import <QTKit/QTkit.h>
#include "VideoSourceMac.h"

FILE *tmp;
BOOL shouldKeepRunning;
#define MICROSECONDS 1000000

/* Objective-C Implementation here */
@interface MozQTCapture : NSObject {
    QTCaptureSession *mSession;
    QTCaptureDeviceInput *mVideo;
    QTCaptureDecompressedVideoOutput *mOutput;
    CVPixelBufferRef mCurrentFrame;
    
    BOOL rec;
    nsIOutputStream *output;
    nsIDOMCanvasRenderingContext2D *vCanvas;
}

- (BOOL)start:(nsIOutputStream *)pipe
    withCanvas:(nsIDOMCanvasRenderingContext2D *)ctx
    width:(int)w andHeight:(int)h;
- (BOOL)stop;
- (void)captureOutput:(QTCaptureOutput *)captureOutput
    didOutputVideoFrame:(CVImageBufferRef)frame
    withSampleBuffer:(QTSampleBuffer*)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection;

@end


@interface MozQTVideoOutput : QTCaptureDecompressedVideoOutput {
    nsIOutputStream *output;
    nsIDOMCanvasRenderingContext2D *canvas;
    NSAutoreleasePool *pool;
}

- (id)initWithStream:(nsIOutputStream *)pipe
    andCanvas:(nsIDOMCanvasRenderingContext2D *)canvas;
- (void)outputVideoFrame:(CVImageBufferRef)videoFrame
    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection;
    
@end

@implementation MozQTVideoOutput

- (id)initWithStream:(nsIOutputStream *)pipe
    andCanvas:(nsIDOMCanvasRenderingContext2D *)ctx
{
    if ((self = [super init])) {
        output = pipe;
        canvas = ctx;
        pool = [[NSAutoreleasePool alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [pool release];
    [super dealloc];
}

- (void)outputVideoFrame:(CVImageBufferRef)videoFrame
    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection
{
    nsresult rv;
    CVBufferRetain(videoFrame);
    CVPixelBufferLockBaseAddress(videoFrame, 0);
    
    void *addr;
    size_t w, h, fsize;
    w = CVPixelBufferGetWidth(videoFrame);
    h = CVPixelBufferGetHeight(videoFrame);
    fsize = (w * h * 3) / 2;
    
    addr = CVPixelBufferGetBaseAddressOfPlane(videoFrame, 0);
    /* each i420 frame is w * h * 3 / 2 bytes long */
    fwrite(addr, fsize, 1, tmp);
    
    /* Write to canvas */
    if (canvas) {
        /* Convert i420 to RGB32 to write on canvas */
        nsAutoArrayPtr<PRUint8> rgb32(new PRUint8[w * h * 4]);
        I420toRGB32(w, h, (const char *)addr, (char *)rgb32.get());
        nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
            canvas, w, h, rgb32, fsize
        );
        rv = NS_DispatchToMainThread(render);
        //fprintf(stderr, "Dispatched to main thread! %d\n", rv);
    }
    
    /* Write to pipe. Bogus timestamp info for now */
    PRUint32 wr;
    PRTime now = PR_Now();
    long now_s = (PRInt32)(now / MICROSECONDS);
    long now_ms = (PRInt32)(now % MICROSECONDS);
    
    output->Write((const char *)&now_s, sizeof(PRInt32), &wr);
    output->Write((const char *)&now_ms, sizeof(PRInt32), &wr);
    output->Write((const char *)&fsize, sizeof(PRUint32), &wr);
    output->Write((const char *)addr, fsize, &wr);
    
    CVPixelBufferUnlockBaseAddress(videoFrame, 0);
    CVBufferRelease(videoFrame);
}

@end

@implementation MozQTCapture

- (BOOL)start:(nsIOutputStream *)pipe
    withCanvas:(nsIDOMCanvasRenderingContext2D *)ctx
    width:(int)w andHeight:(int)h
{
    NSError *error;
    BOOL success = NO;
    mSession = [[QTCaptureSession alloc] init];
    
    QTCaptureDevice *video =
        [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
    success = [video open:&error];
    
    if (!success) {
        NSLog(@"Could not acquire device!\n");
        return NO;
    }
    
    NSLog(@"Acquired %@", [video localizedDisplayName]);

    mVideo = [[QTCaptureDeviceInput alloc] initWithDevice:video];
    success = [mSession addInput:mVideo error:&error];
    
    if (!success) {
        NSLog(@"Could not add input to session!");
        return NO;
    }
    
    mOutput = [[MozQTVideoOutput alloc] initWithStream:pipe andCanvas:ctx];
    [mOutput setDelegate:self];
    [mOutput setAutomaticallyDropsLateVideoFrames:YES];
    
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithDouble:w], (id)kCVPixelBufferWidthKey,
        [NSNumber numberWithDouble:h], (id)kCVPixelBufferHeightKey,
        [NSNumber numberWithUnsignedInt:kCVPixelFormatType_420YpCbCr8Planar],
        //[NSNumber numberWithUnsignedInt:kCVPixelFormatType_422YpCbCr8],
        (id)kCVPixelBufferPixelFormatTypeKey,
        nil];
    [mOutput setPixelBufferAttributes:attributes];

    success = [mSession addOutput:mOutput error:&error];
    if (!success) {
        NSLog(@"Could not add output to session!");
        return NO;
    }
    
    output = pipe;
    vCanvas = ctx;

    tmp = fopen("/Users/anant/Code/tmp/qtkit/test.raw", "w+");
    [mSession startRunning];
    rec = YES;
    NSLog(@"Began session %d!", [mSession isRunning]);
    return YES;
}

- (BOOL)stop
{
    [mSession stopRunning];
    rec = NO;
    fclose(tmp);

    if ([[mVideo device] isOpen])
        [[mVideo device] close];
    
    shouldKeepRunning = NO;
    NSLog(@"Ended session %d!", [mSession isRunning]);
    return YES;
}

- (void)dealloc
{
    [mSession release];
    [mVideo release];
    [mOutput release];
    
    [super dealloc];
}

- (void)captureOutput:(QTCaptureOutput *)captureOutput
    didOutputVideoFrame:(CVImageBufferRef)frame
    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection
{
    NSLog(@"CALLBACK! %@", [NSRunLoop currentRunLoop]);
}

@end

/* C++ wrapper */
VideoSourceMac::VideoSourceMac(int w, int h)
    : VideoSource(w, h)
{
    fps_n = 15;
    fps_d = 1;
    g2g = PR_TRUE;
}

VideoSourceMac::~VideoSourceMac()
{
    [(id)objc release];
}

nsresult
VideoSourceMac::Start(nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    objc = [[MozQTCapture alloc] init];    
    output = pipe;
    canvas = ctx;

    thread = PR_CreateThread(
        PR_SYSTEM_THREAD,
        VideoSourceMac::ReallyStart, this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_JOINABLE_THREAD, 0
    );
    
    return NS_OK;
}

void
VideoSourceMac::ReallyStart(void *data)
{
    VideoSourceMac *vsm = static_cast<VideoSourceMac*>(data);
    
    NSRunLoop *loop = [NSRunLoop currentRunLoop];
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    if ([(id)vsm->objc start:vsm->output withCanvas:vsm->canvas width:vsm->width andHeight:vsm->height]) {
        NSLog(@"Started MozQTCapture!");
        NSLog(@"The loop is: %@", loop);
        
        shouldKeepRunning = YES;
        while (shouldKeepRunning) {
            [loop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
        };
        
        NSLog(@"Run loop returned!!");
    }
    
    [pool drain];
}

nsresult
VideoSourceMac::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    MozQTCapture *mqtc = (id)objc;
    if ([mqtc stop]) {
        [pool drain];
        return NS_OK;
    }
    [pool drain];
    return NS_ERROR_FAILURE;
}

