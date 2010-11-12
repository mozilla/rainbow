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

#include "VideoSourceWin.h"

/*
 * Try to intelligently fetch a default video input device
 */
static HRESULT
GetDefaultInputDevice(IBaseFilter **ppSrcFilter)
{
    HRESULT hr = S_OK;
    IBaseFilter *pSrc = NULL;
    IMoniker *pMoniker = NULL;
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pClassEnum = NULL;

    if (!ppSrcFilter) {
        return E_POINTER;
    }
   
    hr = CoCreateInstance(
        CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
        IID_ICreateDevEnum, (void **)&pDevEnum
    );
    if (FAILED(hr)) return hr;
    
    hr = pDevEnum->CreateClassEnumerator(
        CLSID_VideoInputDeviceCategory, &pClassEnum, 0
    );
    if (FAILED(hr)) {
        SAFE_RELEASE(pDevEnum);
        return hr;
    }
    
    if (pClassEnum == NULL) {
        // No devices available
        SAFE_RELEASE(pDevEnum);
        return E_FAIL;
    }

    // Pick the first device from the list.
    // Note that if the Next() call succeeds but there are no monikers,
    // it will return S_FALSE (which is not a failure).
    hr = pClassEnum->Next (1, &pMoniker, NULL);
    if (hr == S_FALSE) {
        SAFE_RELEASE(pDevEnum);
        SAFE_RELEASE(pClassEnum);
        return E_FAIL;
    }
    
    hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pSrc);
    if (FAILED(hr)) {
        SAFE_RELEASE(pDevEnum);
        SAFE_RELEASE(pClassEnum);
        SAFE_RELEASE(pMoniker);
        return hr;
    }

    *ppSrcFilter = pSrc;
    (*ppSrcFilter)->AddRef();

    SAFE_RELEASE(pSrc);
    SAFE_RELEASE(pMoniker);
    SAFE_RELEASE(pDevEnum);
    SAFE_RELEASE(pClassEnum);

    return hr;
}

VideoSourceWin::VideoSourceWin(int n, int d, int w, int h)
    : VideoSource(n, d, w, h)
{
    HRESULT hr;
    g2g = PR_FALSE;

    hr = CoInitialize(0);
    if (FAILED(hr)) return;

    hr = CoCreateInstance(
        CLSID_FilterGraph, NULL, CLSCTX_INPROC,
        IID_IGraphBuilder, (void **) &pGraph
    );
    if (FAILED(hr)) return;

    hr = CoCreateInstance(
        CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,
        IID_ICaptureGraphBuilder2, (void **) &pCapture
    );
    if (FAILED(hr)) return;

    hr = pGraph->QueryInterface(IID_IMediaControl, (LPVOID *) &pMC);
    if (FAILED(hr)) return;
    hr = pCapture->SetFiltergraph(pGraph);
    if (FAILED(hr)) return;

    hr = GetDefaultInputDevice(&pSrcFilter);
    if (FAILED(hr)) return;

    // Add capture device to graph
    hr = pGraph->AddFilter(pSrcFilter, L"Video Capture");
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }

    // Create SampleGrabber
    hr = CoCreateInstance(
        CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pGrabberF)
    );
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }
    hr = pGrabberF->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }
 
    // Create Null Renderer (required sink by ISampleGrabber)
    hr = CoCreateInstance(
        CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void **)&pNullF
    );
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }

    // Instance initialized properly
    g2g = PR_TRUE;
}

VideoSourceWin::~VideoSourceWin()
{
    SAFE_RELEASE(pMC);
    SAFE_RELEASE(pNullF);
    SAFE_RELEASE(pGraph);
    SAFE_RELEASE(pCapture);
    SAFE_RELEASE(pGrabber);
    SAFE_RELEASE(pGrabberF);

    CoUninitialize();
}

nsresult
VideoSourceWin::Start(nsIOutputStream *pipe)
{
    HRESULT hr;
    AM_MEDIA_TYPE *pCapmt;
    VIDEOINFOHEADER *pVid;
    IAMStreamConfig *pConfig;
    
    if (!g2g)
        return NS_ERROR_FAILURE;

    hr = pCapture->FindInterface(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pSrcFilter,
        IID_IAMStreamConfig, (void**)&pConfig
    );
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return NS_ERROR_FAILURE;
    }
    
    hr = pConfig->GetFormat(&pCapmt);

    pCapmt->majortype = MEDIATYPE_Video;
    pCapmt->subtype = MEDIASUBTYPE_ARGB32;
    pCapmt->formattype = FORMAT_VideoInfo;

    pVid = reinterpret_cast<VIDEOINFOHEADER*>(pCapmt->pbFormat);
    pVid->bmiHeader.biWidth = width;
    pVid->bmiHeader.biHeight = -height;
    pVid->AvgTimePerFrame = NANOSECONDS * (fps_n / fps_d);

    pConfig->SetFormat(pCapmt);
    pGrabber->SetMediaType(pCapmt);
    pGrabber->SetBufferSamples(TRUE);
    
    // Set our callback
    cb = new VideoSourceWinCallback(pipe);
    hr = pGrabber->SetCallback(cb, TYPE_BUFFERCB);
    
    // Add Sample Grabber to graph
    hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
    if (FAILED(hr)) {
        SAFE_RELEASE(pConfig);
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

    // Add Null Renderer to graph
    hr = pGraph->AddFilter(pNullF, L"Null Filter");
    if (FAILED(hr)) {
        SAFE_RELEASE(pConfig);
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

    // Connect capture pin to sample grabber, and that to null renderer
    hr = pCapture->RenderStream(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
        pSrcFilter, pGrabberF, pNullF
    );
    if (FAILED(hr)) {
        SAFE_RELEASE(pConfig);
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

    SAFE_RELEASE(pConfig);
    SAFE_RELEASE(pSrcFilter);

    hr = pMC->Run();
    if (FAILED(hr)) return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
VideoSourceWin::Stop()
{
    if (!g2g)
        return NS_ERROR_FAILURE;

    pMC->StopWhenReady();
    delete cb;
    return NS_OK;
}

// Callback class
VideoSourceWinCallback::VideoSourceWinCallback(nsIOutputStream *pipe)
{
    output = pipe;
    m_refCount = 0;
}

STDMETHODIMP_(ULONG)
VideoSourceWinCallback::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG)
VideoSourceWinCallback::Release()
{
    if (!m_refCount) {
        return 0;
    }
    return --m_refCount;
}

STDMETHODIMP
VideoSourceWinCallback::QueryInterface(REFIID riid, void **ppvObject)
{
    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(this);
        return S_OK;
    }
    
    if (riid == m_IID_ISampleGrabberCB) {
        *ppvObject = static_cast<ISampleGrabberCB*>(this);
        return S_OK;
    }
    
    return E_NOTIMPL;
}

STDMETHODIMP
VideoSourceWinCallback::SampleCB(double Time, IMediaSample *pSample)
{
    return E_NOTIMPL;
}

STDMETHODIMP
VideoSourceWinCallback::BufferCB(double Time, BYTE *pBuffer, long bufferLen)
{
    nsresult rv;
    PRUint32 wr, tmp;
    PRInt32 start, end, offset;
    
    // We always get inverted RGB values. Fix until we switch to i420
    PRInt32 bytesPerPixel = 4;
    for (start = 0, end = bufferLen - bytesPerPixel; start < bufferLen / 2;
        start += bytesPerPixel, end -= bytesPerPixel) {
        
        for (offset = 0; offset < bytesPerPixel; offset += 1) {
            tmp = pBuffer[start + offset];
            pBuffer[start + offset] = pBuffer[end + offset];
            pBuffer[end + offset] = tmp;
        }
    }
    rv = output->Write((const char *)pBuffer, bufferLen, &wr);
    return S_OK;
}
