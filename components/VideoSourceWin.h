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
 *   Brian Coleman <brianfcoleman@gmail.com>
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

/*
 * Video source implementation for Windows.
 * We use the deprecated ISampleGrabber interface,
 * and thus we include parts of qedit.h here.
 */

#include "VideoSource.h"
#include <windows.h>
#include <dshow.h>

#define TYPE_BUFFERCB   1
#define NANOSECONDS     10000000

/* Begin qedit.h */
interface ISampleGrabberCB : public IUnknown
{
    virtual STDMETHODIMP SampleCB( double SampleTime, IMediaSample *pSample ) = 0;
    virtual STDMETHODIMP BufferCB( double SampleTime, BYTE *pBuffer, long BufferLen ) = 0;
};

static const
IID IID_ISampleGrabberCB = { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };

interface ISampleGrabber : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetOneShot( BOOL OneShot ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType( const AM_MEDIA_TYPE *pType ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( AM_MEDIA_TYPE *pType ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples( BOOL BufferThem ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer( long *pBufferSize, long *pBuffer ) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample( IMediaSample **ppSample ) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback( ISampleGrabberCB *pCallback, long WhichMethodToCallback ) = 0;
};

static const
IID IID_ISampleGrabber = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
static const
CLSID CLSID_SampleGrabber = { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };
static const
CLSID CLSID_NullRenderer = { 0xC1F400A4, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

#define SAFE_RELEASE(x) { if (x) x->Release(); x = NULL; }
/* End qedit.h */

class VideoSourceWinCallback : public ISampleGrabberCB
{
public:
    VideoSourceWinCallback(nsIOutputStream *pipe);

    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
    STDMETHODIMP SampleCB(double Time, IMediaSample *pSample);
    STDMETHODIMP BufferCB(double Time, BYTE *pBuffer, long BufferLen);

private:
    int m_refCount;
    nsIOutputStream *output;
    IID m_IID_ISampleGrabberCB;

};

class VideoSourceWin : public VideoSource
{
public:
    VideoSourceWin(int w, int h);
    ~VideoSourceWin();

    nsresult Stop();
    nsresult Start(nsIOutputStream *pipe);

protected:
    IMediaControl *pMC;
    IGraphBuilder *pGraph;
    ICaptureGraphBuilder2 *pCapture;
    
    IBaseFilter *pNullF;
    IBaseFilter *pGrabberF;
    IBaseFilter *pSrcFilter;
    ISampleGrabber *pGrabber;

    AM_MEDIA_TYPE *pMT;
    IAMStreamConfig *pConfig;
    
    VideoSourceWinCallback *cb;

};
