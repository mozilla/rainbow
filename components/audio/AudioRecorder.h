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
 * The Original Code is Audio Recorder.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Labs
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef AudioRecorder_h_
#define AudioRecorder_h_

#include "IAudioRecorder.h"

#include <ogg/ogg.h>
#include <portaudio.h>
#include <vorbis/vorbisenc.h>

#include "prmem.h"
#include "prthread.h"

#include "nsIPipe.h"
#include "nsStringAPI.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"

#define AUDIO_RECORDER_CONTRACTID "@labs.mozilla.com/audio/recorder;1"
#define AUDIO_RECORDER_CLASSNAME  "Audio Recording Capability"
#define AUDIO_RECORDER_CID { 0x1fdf790f, 0x0648, 0x4e53, \
                           { 0x92, 0x7d, 0xbe, 0x13, 0xa3, 0xc6, 0x92, 0x54 } }

#define SAMPLE_RATE         44100
#define FRAMES_BUFFER       1024
#define NUM_CHANNELS        2
#define SAMPLE_FORMAT       paInt16
#define SAMPLE_QUALITY      0.4

typedef struct {
	ogg_page og;
	ogg_packet op;
	ogg_stream_state os;
} ogg_state;

typedef struct {
	vorbis_info vi;
	vorbis_block vb;
	vorbis_comment vc;
	vorbis_dsp_state vd;
} vorbis_state;

class AudioRecorder : public IAudioRecorder
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IAUDIORECORDER

    nsresult Init();
    static AudioRecorder *GetSingleton();
    virtual ~AudioRecorder();
    AudioRecorder(){}
    
    
private:
    int recording;
	FILE *outfile;
    PRThread *encthr;
    PaStream *stream;
	
	ogg_state *odata;
	vorbis_state *vdata;
       
    nsCOMPtr<nsIAsyncInputStream> mPipeIn;
    nsCOMPtr<nsIAsyncOutputStream> mPipeOut;

    static AudioRecorder *gAudioRecordingService;

protected:
    static void Encode(void *data);
    nsresult SetupOggVorbis(nsACString& file);
    static int Callback(const void *input, void *output,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *data
    );
};

#endif
