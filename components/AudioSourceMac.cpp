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

#include "AudioSourceMac.h"

/*
 * Try to intelligently fetch a default audio input device
 */
static PaDeviceIndex
GetDefaultInputDevice()
{
    int i, n;
    PaDeviceIndex def;
    const PaDeviceInfo *deviceInfo;
    
    n = Pa_GetDeviceCount();
    if (n < 0) {
        return paNoDevice;
    }
    
    /* Try default input */
    if ((def = Pa_GetDefaultInputDevice()) != paNoDevice) {
        return def;
    }
    
    /* No luck, iterate and check for API specific input device */
    for (i = 0; i < n; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {
            return i;
        }
    }
    /* No device :( */
    return paNoDevice;
}

AudioSourceMac::AudioSourceMac(int c, int r)
    : AudioSource(c, r)
{
    stream = NULL;
    if (Pa_Initialize() != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not initialize PortAudio\n"));
        return;
    }
    source = GetDefaultInputDevice();

    if (source == paNoDevice) {
        PR_LOG(log, PR_LOG_NOTICE, ("No audio devices found!\n"));
    }
}

AudioSourceMac::~AudioSourceMac()
{
    Pa_Terminate();
}

nsresult
AudioSourceMac::Start(nsIOutputStream *pipe)
{
    PaError err;
    PaStreamParameters param;    

    param.device = source;
    param.channelCount = channels;
    param.sampleFormat = paInt16;
    param.suggestedLatency =
        Pa_GetDeviceInfo(source)->defaultLowInputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream, &param, NULL, rate, FRAMES_BUFFER,
        paClipOff, AudioSourceMac::Callback, this
    );

    if (err != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not open stream! %d", err));
        return NS_ERROR_FAILURE;
    }

    if (Pa_StartStream(stream) != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not start stream!"));
        return NS_ERROR_FAILURE;
    }
   
    output = pipe;
    return NS_OK;
}

nsresult
AudioSourceMac::Stop()
{
    if (Pa_StopStream(stream) != paNoError) {
        PR_LOG(log, PR_LOG_NOTICE, ("Could not close stream!\n"));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

int
AudioSourceMac::Callback(const void *input, void *output,
    unsigned long frames, const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags, void *data)
{
    nsresult rv;
    PRUint32 wr;
    AudioSourceMac *asa = static_cast<AudioSourceMac*>(data);

    /* Write to pipe and return quickly */
    rv = asa->output->Write(
        (const char *)input, frames * asa->GetFrameSize(), &wr
    );
    return paContinue;
}

