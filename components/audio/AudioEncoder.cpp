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
 * The Original Code is Audio Encoder.
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

#include "AudioEncoder.h"

NS_IMPL_ISUPPORTS1(AudioEncoder, IAudioEncoder)

AudioEncoder::AudioEncoder()
{
    encoding = 0;
    outfile = NULL;
}

AudioEncoder::~AudioEncoder()
{
}

/*
 * This code is ripped from profile/src/nsProfile.cpp and is further
 * duplicated in uriloader/exthandler.  this should probably be moved
 * into xpcom or some other shared library.
 */ 
static void
MakeRandomString(char *buf, PRInt32 bufLen)
{
    // turn PR_Now() into milliseconds since epoch
    // and salt rand with that.
    double fpTime;
    LL_L2D(fpTime, PR_Now());

    // use 1e-6, granularity of PR_Now() on the mac is seconds
    srand((uint)(fpTime * 1e-6 + 0.5));   
    PRInt32 i;
    for (i=0;i<bufLen;i++) {
        *buf++ = table[rand()%TABLE_SIZE];
    }
    *buf = 0;
}

/*
 * Create and open OGG file
 */
NS_IMETHODIMP
AudioEncoder::CreateOgg(nsACString& file)
{
	if (encoding) {
		fprintf(stderr, "JEP Audio:: Encoding in progress!\n");
		return NS_ERROR_FAILURE;
	}
	
	nsresult rv;
	nsCOMPtr<nsIFile> o;

    /* Allocate OGG file */
    char buf[13];
    nsCAutoString path;

    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(o));
    if (NS_FAILED(rv)) return rv;

    MakeRandomString(buf, 8);
    memcpy(buf + 8, ".ogg", 5);
    rv = o->AppendNative(nsDependentCString(buf, 12));
    if (NS_FAILED(rv)) return rv;
    rv = o->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv)) return rv;
    rv = o->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;
    rv = o->Remove(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    /* Open file in libsndfile */
    SF_INFO info;
    info.channels = NUM_CHANNELS;
    info.samplerate = SAMPLE_RATE;
    info.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;

    if (!(outfile = sf_open(path.get(), SFM_WRITE, &info))) {
        sf_perror(NULL);
        return NS_ERROR_FAILURE;
    }

	file.Assign(path.get(), strlen(path.get()));
	
	encoding = 1;
	return NS_OK;
}

/*
 * Append a single stereo frame to the ogg file
 */
NS_IMETHODIMP
AudioEncoder::AppendFrames(PRInt32 *frames, PRUint32 numBytes)
{	
	if (encoding != 1) {
		fprintf(stderr, "JEP Audio:: Encoding did not begin, cannot append!\n");
		return NS_ERROR_FAILURE;
	}
	
    if (numBytes % (NUM_CHANNELS * sizeof(SAMPLE)) != 0) {
        fprintf(stderr, "JEP Audio:: Frame count not multiple of channels!"
                        " %d\n", numBytes);
        return NS_ERROR_FAILURE;
    }

    PRUint32 fr = numBytes / (NUM_CHANNELS * sizeof(SAMPLE));
    if (sf_writef_int(outfile, (const SAMPLE *)frames, fr) != fr) {
        fprintf(stderr, "JEP Audio:: Could not append frames!\n");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/*
 * Close SNDFILE
 */
NS_IMETHODIMP
AudioEncoder::Finalize()
{
	if (encoding != 1) {
		fprintf(stderr, "JEP Audio:: Encoding did not begin, cannot finalize! %d\n", encoding);
		return NS_ERROR_FAILURE;
	}
	
	sf_close(outfile);
	
	encoding = 0;
	outfile = NULL;
	return NS_OK;
}