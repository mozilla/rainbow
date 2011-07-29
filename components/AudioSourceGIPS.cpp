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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ralph Giles <giles@mozilla.com>
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

#include "AudioSourceGIPS.h"

/* 1000000 us = 1 sec. */
#define MICROSECONDS 1000000

AudioSourceGIPS::AudioSourceGIPS(int c, int r) :
    AudioSource::AudioSource(c, r)
{
    int error = 0;

    ptrVoE = webrtc::VoiceEngine::Create();
    if (ptrVoE == NULL) {
        fprintf(stderr, "ERROR in GIPSVoiceEngine::Create\n");
        return;
    }

    error = ptrVoE->SetTraceFile("GIPSVoETrace.txt");
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSVoiceEngine::SetTraceFile\n");
        return;
    }

    ptrVoEBase = webrtc::VoEBase::GetInterface(ptrVoE);
    if (ptrVoEBase == NULL) {
        fprintf(stderr, "ERROR in GIPSVoEBase::GetInterface\n");
        return;
    }
    error = ptrVoEBase->Init();
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSVoEBase::Init\n");
        return;
    }

    channel = ptrVoEBase->CreateChannel();
    if (channel < 0) {
        fprintf(stderr, "ERROR in GIPSVoEBase::CreateChannel\n");
    }

    ptrVoEHardware = webrtc::VoEHardware::GetInterface(ptrVoE);
    if (ptrVoEHardware == NULL) {
        fprintf(stderr, "ERROR in VoEHardware::GetInterface\n");
    }

    int nDevices;
    error = ptrVoEHardware->GetNumOfRecordingDevices(nDevices);
    fprintf(stderr, "Got %d recording devices devices!\n", nDevices);
    for (int i = 0; i < nDevices; i++) {
        char name[128], guid[128];
        memset(name, 0, 128);
        memset(guid, 0, 128);
        error = ptrVoEHardware->GetRecordingDeviceName(i, name, guid);
        if (error) {
            fprintf(stderr, "ERROR in VoEHardware::GetRecordingDeviceName\n");
        } else {
            fprintf(stderr, "%d\t%s (%s)\n", i, name, guid);
        }
    }

    fprintf(stderr, "setting default recording device\n");
    error = ptrVoEHardware->SetRecordingDevice(0);
    if (error) {
        fprintf(stderr, "ERROR in VoEHardware::SetRecordingDevice\n");
    }

    bool recAvail = false;
    ptrVoEHardware->GetRecordingDeviceStatus(recAvail);
    fprintf(stderr, "Recording device is %savailable\n",
        recAvail ? "" : "NOT ");

    ptrVoERender = webrtc::VoEExternalMedia::GetInterface(ptrVoE);
    if (ptrVoERender == NULL) {
        fprintf(stderr, "ERROR in GIPSVoEExernalMedia::GetInterface\n");
        return;
    }

    /* NB: we must set a send destination and call StartSend
       before the capture pipeline will run. Starting the
       receiver is optional, but nice for the demo because
       it feeds a monitor signal to the playout device. */
    ptrVoEBase->SetSendDestination(channel, 8000, "127.0.0.1");
    ptrVoEBase->SetLocalReceiver(channel, 8000);

    strcpy(codec.plname, "PCMU");
    codec.channels = c;
    codec.pacsize = 160;
    codec.plfreq = 8000;
    codec.pltype = 0;
    codec.rate = r;
}

AudioSourceGIPS::~AudioSourceGIPS()
{
    int error;

    error = ptrVoEBase->DeleteChannel(channel);
    if (error == -1) {
        fprintf(stderr, "ERROR in GIPSVoEBase::DeleteChannel\n");
    }

    ptrVoEBase->Terminate();
    ptrVoEBase->Release();
    ptrVoEHardware->Release();
    ptrVoERender->Release();

    if (webrtc::VoiceEngine::Delete(ptrVoE) == false) {
        fprintf(stderr, "ERROR in GIPSVoiceEngine::Delete\n");
    }
}

nsresult
AudioSourceGIPS::Start(nsIOutputStream *pipe)
{
    int error;
    output = pipe;

    error = ptrVoEBase->StartReceive(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StartReceive\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoEBase->StartPlayout(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StartPlayout\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoEBase->StartSend(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StartSend\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoERender->RegisterExternalMediaProcessing(channel,
        webrtc::kRecordingPerChannel, *this);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEExternalMedia::RegisterExternalMediaProcessing\n");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
AudioSourceGIPS::Stop()
{
    int error;

    error = ptrVoERender->DeRegisterExternalMediaProcessing(channel,
        webrtc::kRecordingPerChannel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEExternalMedia::DeRegisterExternalMediaProcessing\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoEBase->StopSend(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StopSend\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoEBase->StopReceive(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StopReceive\n");
        return NS_ERROR_FAILURE;
    }

    error = ptrVoEBase->StopPlayout(channel);
    if (error) {
        fprintf(stderr, "ERROR in GIPSVoEBase::StopPlayout\n");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void AudioSourceGIPS::Process(const int channel,
             const webrtc::ProcessingTypes type,
             WebRtc_Word16 audio10ms[], const int length,
             const int samplingFreq, const bool isStereo)
{
    nsresult rv;
    PRUint32 wr;
    fprintf(stderr, "Audio frame buffer %08llx\t %d\t %d Hz %s\n",
            (unsigned long long)audio10ms, length, samplingFreq,
            isStereo ? "stereo" : "mono");

    /* Write to pipe and return quickly */
    rv = output->Write(
        (const char *)audio10ms, length, &wr
    );
    return;
}
