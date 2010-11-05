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
 * The Original Code is Video for Jetpack.
 *
 * The Initial Developer of the Original Code is Mozilla Labs.
 * Portions created by the Initial Developer are Copyright (C) 2009-10
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

#ifndef MediaRecorder_h_
#define MediaRecorder_h_

#include "IMediaRecorder.h"

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include <theora/theoraenc.h>

#include <prmem.h>
#include <prthread.h>

#include <nsIPipe.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>
#include <nsIFileStreams.h>
#include <nsIAsyncInputStream.h>
#include <nsIAsyncOutputStream.h>
#include <nsComponentManagerUtils.h>
#include <nsIDOMCanvasRenderingContext2D.h>

/* ifdefs are evil, but I am powerless */
#ifdef RAINBOW_Mac
#include "VideoSourceMac.h"
#endif
#ifdef RAINBOW_Win
#include "VideoSourceWin.h"
#endif

/* We use portaudio for all platforms currently */
#include "AudioSourcePortaudio.h"

#define MEDIA_RECORDER_CONTRACTID "@labs.mozilla.com/media/recorder;1"
#define MEDIA_RECORDER_CID { 0xc467b1f4, 0x551c, 0x4e2f, \
                           { 0xa6, 0xba, 0xcb, 0x7d, 0x79, 0x2d, 0x14, 0x52 }}

typedef struct {
    ogg_page og;
    ogg_packet op;
    vorbis_info vi;
    vorbis_block vb;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    ogg_stream_state os;

    AudioSource *backend;
    nsCOMPtr<nsIAsyncInputStream> aPipeIn;
    nsCOMPtr<nsIAsyncOutputStream> aPipeOut;
} Audio;

typedef struct {
    th_info ti;
    ogg_page og;
    ogg_packet op;
    th_comment tc;
    th_enc_ctx *th;
    ogg_stream_state os;
    
    VideoSource *backend;
    nsCOMPtr<nsIAsyncInputStream> vPipeIn;
    nsCOMPtr<nsIAsyncOutputStream> vPipeOut;
    nsIDOMCanvasRenderingContext2D *vCanvas;
} Video;

typedef struct {
    double qual;
    PRBool audio, video;
    PRUint32 fps_n, fps_d, width, height, rate, chan;
} Properties;

class MediaRecorder : public IMediaRecorder
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IMEDIARECORDER

    nsresult Init();
    static MediaRecorder *GetSingleton();
    virtual ~MediaRecorder();
    MediaRecorder(){}

protected:
    Audio *aState;
    Video *vState;

    PRThread *encoder;
    PRBool a_stp, v_stp;
    PRBool a_rec, v_rec;
    PRLogModuleInfo *log;

    nsresult SetupTheoraBOS();
    nsresult SetupVorbisBOS();
    nsresult SetupTheoraHeaders();
    nsresult SetupVorbisHeaders();

    nsCOMPtr<nsIOutputStream> pipeOut;

    static void Encode(void *data);
    static void WriteAudio(void *data);
    static MediaRecorder *gMediaRecordingService;

    void ParseProperties(nsIPropertyBag2 *prop);
    nsresult Record(nsIDOMCanvasRenderingContext2D *ctx);
    nsresult MakePipe(nsIAsyncInputStream **in, nsIAsyncOutputStream **out);

private:
    Properties *params;

};

#endif
