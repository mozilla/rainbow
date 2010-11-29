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
}

- (void)outputVideoFrame:(CVImageBufferRef)videoFrame
    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection;
    
@end

@implementation MozQTVideoOutput

- (void)outputVideoFrame:(CVImageBufferRef)videoFrame
    withSampleBuffer:(QTSampleBuffer *)sampleBuffer
    fromConnection:(QTCaptureConnection *)connection
{
    CVBufferRetain(videoFrame);
    CVPixelBufferLockBaseAddress(videoFrame, 0);
    
    void *addr;
    size_t w, h;
    w = CVPixelBufferGetWidth(videoFrame);
    h = CVPixelBufferGetHeight(videoFrame);
    
    addr = CVPixelBufferGetBaseAddressOfPlane(videoFrame, 0);
    /* each i420 frame is w * h * 3 / 2 bytes long */
    fwrite(addr, (w * h * 3) / 2, 1, tmp);
    
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
    
    mOutput = [[MozQTVideoOutput alloc] init];
    //[mOutput setDelegate:self];
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
    NSLog(@"CALLBACK!!!! %@", [NSRunLoop currentRunLoop]);
}

@end

/* C++ wrapper */
VideoSourceMac::VideoSourceMac(int w, int h)
    : VideoSource(w, h)
{
    fps_n = 15;
    fps_d = 1;
    
    pool = [[NSAutoreleasePool alloc] init];
    objc = [[MozQTCapture alloc] init];
    g2g = PR_TRUE;
}

VideoSourceMac::~VideoSourceMac()
{
    [(id)objc release];
    [(id)pool release];
}

nsresult
VideoSourceMac::Start(
    nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx)
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    MozQTCapture *mqtc = (id)objc;
    if ([mqtc start:pipe withCanvas:ctx width:width andHeight:height]) {
        NSLog(@"Started MozQTCapture!");
        //[[NSRunLoop mainRunLoop] run];
        return NS_OK;
    } else {
        return NS_ERROR_FAILURE;
    }
}

nsresult
VideoSourceMac::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    MozQTCapture *mqtc = (id)objc;
    if ([mqtc stop]) {
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

