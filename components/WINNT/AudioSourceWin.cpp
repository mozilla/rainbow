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

#include "AudioSourceWin.h"
#define MICROSECONDS 1000000

AudioSourceWin::AudioSourceWin(int c, int r)
    : AudioSource(c, r)
{
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = c;
    format.nSamplesPerSec = r;
    format.wBitsPerSample = sizeof(SAMPLE) * 8;
    format.nBlockAlign = c * sizeof(SAMPLE);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

    rec = PR_FALSE;
}

AudioSourceWin::~AudioSourceWin()
{
    
}

nsresult
AudioSourceWin::Start(nsIOutputStream *pipe)
{
    MMRESULT err;
    output = pipe;
    
    HANDLE cbthread = CreateThread(
        0, 0, (LPTHREAD_START_ROUTINE)Callback, this, 0, &thread
    );
    if (!cbthread) return NS_ERROR_FAILURE;
    CloseHandle(cbthread);
    
    err = waveInOpen(
        &handle, WAVE_MAPPER, &format, thread, (DWORD_PTR)this, CALLBACK_THREAD
    );
    if (err) {
        PR_LOG(log, PR_LOG_ERROR, ("waveInOpen failed with %d\n", err));
        return NS_ERROR_FAILURE;
    }
    
    /* Allocate, prepare and add buffers */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        ZeroMemory(&buffer[i], sizeof(WAVEHDR));
        buffer[i].dwBufferLength = format.nAvgBytesPerSec << 1;
        
        if (!(buffer[i].lpData = (char *) VirtualAlloc(
            0, buffer[i].dwBufferLength, MEM_COMMIT, PAGE_READWRITE
        ))) return NS_ERROR_FAILURE;
        
        if (waveInPrepareHeader(handle, &buffer[i], sizeof(WAVEHDR))) {
            return NS_ERROR_FAILURE;
        }
        
        if (waveInAddBuffer(handle, &buffer[i], sizeof(WAVEHDR))) {
            return NS_ERROR_FAILURE;
        }
    }
    
    /* Establish baseline stream time with absolute time since epoch */
    PRUint32 wr;
    PRTime epoch_c = PR_Now();
    PRFloat64 epoch = (PRFloat64)(epoch_c / MICROSECONDS);
    epoch += ((PRFloat64)(epoch_c % MICROSECONDS)) / MICROSECONDS;
    pipe->Write((const char *)&epoch, sizeof(PRFloat64), &wr);
    
    /* Go! */
    if (waveInStart(handle)) {
        PR_LOG(log, PR_LOG_ERROR, ("waveInStart failed with %d\n", err));
        return NS_ERROR_FAILURE;
    }
    
    rec = PR_TRUE;
    return NS_OK;
}

nsresult
AudioSourceWin::Stop()
{
    /* Notify callback to stop buffering */
    rec = PR_FALSE;
    waveInReset(handle);
    
    /* Wait for Callback to finish writing any pending buffers */
    while (pending != 0) Sleep(100);
    
    /* Clean up headers */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveInPrepareHeader(handle, &buffer[i], sizeof(WAVEHDR));
        if (buffer[i].lpData)
            VirtualFree(buffer[i].lpData, 0, MEM_RELEASE);
    }

    waveInClose(handle);
    return NS_OK;
}

DWORD WINAPI
AudioSourceWin::Callback(void *data)
{
    MSG msg;
    nsresult rv;
    PRUint32 wr;
    WAVEHDR *hdr;
    AudioSourceWin *asw = static_cast<AudioSourceWin*>(data);
    
    PRFloat64 current = 0.0;
    
    /* This MSG comes from the audio driver */
    while (GetMessage(&msg, 0, 0, 0) == 1) {
        switch (msg.message) {
            case MM_WIM_DATA:
                /* A buffer has been filled by the driver */
                hdr = (WAVEHDR *)msg.lParam;
                if (hdr->dwBytesRecorded) {
                    /* Write samples to pipe */
                    rv = asw->output->Write(
                        (const char *)((WAVEHDR *)msg.lParam)->lpData,
                        hdr->dwBytesRecorded, &wr
                    );
                }

                if (asw->rec) {
                    /* If we're still recording, send back this buffer */
                    waveInAddBuffer(
                        asw->handle, (WAVEHDR *)msg.lParam, sizeof(WAVEHDR)
                    );
                } else {
                    asw->pending -= 1;
                }
                continue;
            case MM_WIM_OPEN:
                /* Set pending number of buffers */
                asw->pending = NUM_BUFFERS;
                continue;
            case MM_WIM_CLOSE:
                /* We're done recording */
                break;
        }
    }

    return 0;
}

