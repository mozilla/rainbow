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

#ifndef AudioEncoder_h_
#define AudioEncoder_h_

#include "IAudioEncoder.h"

// MSVC Weirdness
#define __int64_t __int64
#include "sndfile.h"
#undef __int64_t

#include "prmem.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsStringAPI.h"
#include "nsThreadUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"

#define AUDIO_ENCODER_CONTRACTID "@labs.mozilla.com/audio/encoder;1"
#define AUDIO_ENCODER_CLASSNAME  "Audio Encoding Capability"
#define AUDIO_ENCODER_CID { 0xb7182604, 0x7BE6, 0x4308, \
                          { 0x81, 0x0C, 0x12, 0x8F, 0xD7, 0xD7, 0x76, 0xDE } }

#ifndef SAMPLE_RATE
#define SAMPLE_RATE         (44000)
#endif
#ifndef NUM_CHANNELS
#define NUM_CHANNELS        (2)
#endif
#ifndef PA_SAMPLE_TYPE
#define PA_SAMPLE_TYPE      paInt32
typedef int SAMPLE;
#endif

class AudioEncoder : public IAudioEncoder
{
public:
    AudioEncoder();
    NS_DECL_ISUPPORTS
    NS_DECL_IAUDIOENCODER

private:
    ~AudioEncoder();
    int encoding;
    SNDFILE *outfile;

};

#define TABLE_SIZE 36
static const char table[] = {
    'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9' 
};

#endif
