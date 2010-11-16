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

#include "AudioSourceNix.h"

AudioSourceNix::AudioSourceNix(int c, int r)
    : AudioSource(c, r)
{
    int err;
    unsigned int ap_rate = rate;
    unsigned int ap_chan = channels;

    err = snd_pcm_open(&device, ADDR, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) { g2g = PR_FALSE; return; }

    err = snd_pcm_hw_params_malloc(&params);
    if (err < 0) { g2g = PR_FALSE; return; }

    err = snd_pcm_hw_params_any(device, params);
    if (err < 0) { g2g = PR_FALSE; return; }
    
    err = snd_pcm_hw_params_set_access(
        device, params, SND_PCM_ACCESS_RW_INTERLEAVED
    );
    if (err < 0) { g2g = PR_FALSE; return; }

    err = snd_pcm_hw_params_set_format(
        device, params, SND_PCM_FORMAT_S16_LE
    );
    if (err < 0) { g2g = PR_FALSE; return; }

    /* For rate and channels if we don't get
     * what we want, that's too bad */
    err = snd_pcm_hw_params_set_rate_near(
        device, params, &ap_rate, 0
    );
    if (err < 0) {
        err = snd_pcm_hw_params_get_rate(
            params, &ap_rate, 0
        );
        if (err < 0) { g2g = PR_FALSE; return; }
    }

    err = snd_pcm_hw_params_set_channels(
        device, params, ap_chan
    );
    if (err < 0) {
        err = snd_pcm_hw_params_get_channels(
            params, &ap_chan
        );
        if (err < 0) { g2g = PR_FALSE; return; }
    }

    g2g = PR_TRUE;
    rec = PR_FALSE;
    rate = ap_rate;
    channels = ap_chan;
    snd_pcm_close(device);
}

AudioSourceNix::~AudioSourceNix()
{
    snd_pcm_hw_params_free(params);
}

nsresult
AudioSourceNix::Start(nsIOutputStream *pipe)
{
    int err;
    output = pipe;
    
    if (!g2g) return NS_ERROR_FAILURE;

    err = snd_pcm_open(&device, ADDR, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) return NS_ERROR_FAILURE;

    err = snd_pcm_hw_params(device, params);
    if (err < 0) return NS_ERROR_FAILURE;

    rec = PR_TRUE;
    capture = PR_CreateThread(
        PR_SYSTEM_THREAD, AudioSourceNix::CaptureThread, this,
        PR_PRIORITY_HIGH, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0
    );

    return NS_OK;
}

nsresult
AudioSourceNix::Stop()
{
    rec = PR_FALSE;
    PR_JoinThread(capture);
    snd_pcm_close(device);

    return NS_OK;
}

void
AudioSourceNix::CaptureThread(void *data)
{
    int err;
    nsresult rv;
    PRUint32 wr;
    SAMPLE *buffer;
    int frames_read;
    signed short *ptr;
    AudioSourceNix *asn = static_cast<AudioSourceNix*>(data);

    buffer = (short *)PR_Calloc(asn->GetFrameSize(), FRAMES_BUFFER);
    while (asn->rec) {
        ptr = buffer;
        frames_read = 0;

        while (frames_read < FRAMES_BUFFER && asn->rec) {
            err = snd_pcm_readi(
                asn->device, ptr, FRAMES_BUFFER - frames_read
            );
            if (err < 0) {
                // uh-oh
                PR_Free(buffer);
                return;
            }

            frames_read += err;
            ptr += err * asn->channels;
        }

        rv = asn->output->Write(
            (const char*)buffer, asn->GetFrameSize() * frames_read, &wr
        );
    }
    PR_Free(buffer);
}

