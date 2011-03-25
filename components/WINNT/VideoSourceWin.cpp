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
#include "nsStringAPI.h"
#define MICROSECONDS 1000000

IID IID_ISampleGrabber;
IID IID_ISampleGrabberCB;
CLSID CLSID_SampleGrabber;
CLSID CLSID_NullRenderer;

static NS_NAMED_LITERAL_STRING(kFilenameQeditTypeLib, "qedit.dll");
static NS_NAMED_LITERAL_STRING(kNameISampleGrabber, "ISampleGrabber");
static NS_NAMED_LITERAL_STRING(kNameISampleGrabberCB, "ISampleGrabberCB");
static NS_NAMED_LITERAL_STRING(kNameClassSampleGrabber, "SampleGrabber");
static NS_NAMED_LITERAL_STRING(kNameClassNullRenderer, "NullRenderer");

/* Extract GUID with name nameGUID and assign it to guid */
static void
InitTypeLibGUID(
    ITypeLib* const pTypeLib, ITypeInfo** const typeInfoPtrArray,
    MEMBERID* const memberIdArray, const nsAString& nameGUID,
    const TYPEKIND typeKindGUID, GUID& guid) {
    
    // Copy nameGUID as FindName requires a non const string
    nsString copyNameGUID(nameGUID);
    PRInt32 hashValue = 0;
    PRUint16 countFound = 1;
    HRESULT result = pTypeLib->FindName(
        copyNameGUID.BeginWriting(), hashValue, typeInfoPtrArray, 
        memberIdArray, &countFound
    );
    
    if (result == S_OK) {
        for (PRUint16 i = 0; i < countFound; ++i) {
            ITypeInfo* pTypeInfo = typeInfoPtrArray[i];
            typeInfoPtrArray[i] = 0;
            TYPEATTR* pTypeAttribute = 0;
            
            result = pTypeInfo->GetTypeAttr(&pTypeAttribute);
            if (result == S_OK) {
                if (pTypeAttribute->typekind == typeKindGUID) {
                    guid = pTypeAttribute->guid;
                }
                pTypeInfo->ReleaseTypeAttr(pTypeAttribute);
            }
        
            SAFE_RELEASE(pTypeInfo);
        }
    }
}

/* Extract IIDs and CLSIDs from qedit type library */
static void
InitAllQeditTypeLibGUIDs()
{
    ITypeLib* pQeditTypeLib = 0;
    HRESULT result = LoadTypeLib(
        kFilenameQeditTypeLib.BeginReading(), &pQeditTypeLib
    );
    
    if (result == S_OK) {
        PRUint32 countTypeInfo = pQeditTypeLib->GetTypeInfoCount();
        ITypeInfo** typeInfoPtrArray = new ITypeInfo*[countTypeInfo];
        MEMBERID* memberIdArray = new MEMBERID[countTypeInfo];
        
        InitTypeLibGUID(
            pQeditTypeLib, typeInfoPtrArray, memberIdArray,
            kNameISampleGrabber, TKIND_INTERFACE, IID_ISampleGrabber
        );
        InitTypeLibGUID(
            pQeditTypeLib, typeInfoPtrArray, memberIdArray,
            kNameISampleGrabberCB, TKIND_INTERFACE, IID_ISampleGrabberCB
        );
        InitTypeLibGUID(
            pQeditTypeLib, typeInfoPtrArray, memberIdArray,
            kNameClassSampleGrabber, TKIND_COCLASS, CLSID_SampleGrabber
        );
        InitTypeLibGUID(
            pQeditTypeLib, typeInfoPtrArray, memberIdArray,
            kNameClassNullRenderer, TKIND_COCLASS, CLSID_NullRenderer
        );
        
        delete[] memberIdArray;
        delete[] typeInfoPtrArray;
    }
    
    SAFE_RELEASE(pQeditTypeLib);
}

/* Release the format block for a media type */
static void
_FreeMediaType(AM_MEDIA_TYPE &mt)
{
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        /* pUnk should not be used. */
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

/* Delete a media type structure that was allocated on the heap */
static void
_DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
    if (pmt != NULL)
    {
        _FreeMediaType(*pmt);
        CoTaskMemFree(pmt);
    }
}

/* Try to intelligently fetch a default video input device */
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
        /* No devices available */
        SAFE_RELEASE(pDevEnum);
        return E_FAIL;
    }

    /* Pick the first device from the list.
     * Note that if the Next() call succeeds but there are no monikers,
     * it will return S_FALSE (which is not a failure).
     */
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

VideoSourceWin::VideoSourceWin(int w, int h)
    : VideoSource(w, h)
{
    HRESULT hr;
    VIDEOINFOHEADER *pVid;

    g2g = PR_FALSE;

    hr = CoInitialize(0);
    if (FAILED(hr)) return;

    InitAllQeditTypeLibGUIDs();

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

    /* Add capture device to graph */
    hr = pGraph->AddFilter(pSrcFilter, L"Video Capture");
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }

    /* Create SampleGrabber */
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

    /* Create Null Renderer (required sink by ISampleGrabber) */
    hr = CoCreateInstance(
        CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void **)&pNullF
    );
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }

    /* Get default media type values and set what we need */
    hr = pCapture->FindInterface(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pSrcFilter,
        IID_IAMStreamConfig, (void**)&pConfig
    );
    if (FAILED(hr)) {
        pSrcFilter->Release();
        return;
    }

    hr = pConfig->GetFormat(&pMT);

    /* We play it safe and request RGB32 always because that's what majority
     * of webcams seem to support
     */
    pMT->majortype = MEDIATYPE_Video;
    pMT->formattype = FORMAT_VideoInfo;
    pMT->subtype = MEDIASUBTYPE_ARGB32;
    pVid = reinterpret_cast<VIDEOINFOHEADER*>(pMT->pbFormat);
    pVid->bmiHeader.biWidth = width;
    pVid->bmiHeader.biHeight = height;

    /* Set frame rates */
    fps_n = NANOSECONDS;
    fps_d = (PRUint32)pVid->AvgTimePerFrame;

    pConfig->SetFormat(pMT);
    pGrabber->SetMediaType(pMT);
    _DeleteMediaType(pMT);

    /* Instance initialized properly */
    SAFE_RELEASE(pConfig);
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
VideoSourceWin::Start(
    nsIOutputStream *pipe, nsIDOMCanvasRenderingContext2D *ctx)
{
    HRESULT hr;

    if (!g2g)
        return NS_ERROR_FAILURE;

    /* Set our callback */
    pGrabber->SetBufferSamples(TRUE);
    cb = new VideoSourceWinCallback(pipe, width, height, ctx);
    hr = pGrabber->SetCallback(cb, TYPE_SAMPLECB);

    /* Add Sample Grabber to graph */
    hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
    if (FAILED(hr)) {
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

    /* Add Null Renderer to graph */
    hr = pGraph->AddFilter(pNullF, L"Null Filter");
    if (FAILED(hr)) {
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

    /* Connect capture pin to sample grabber, and that to null renderer */
    hr = pCapture->RenderStream(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
        pSrcFilter, pGrabberF, pNullF
    );
    if (FAILED(hr)) {
        SAFE_RELEASE(pSrcFilter);
        return NS_ERROR_FAILURE;
    }

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

/* Callback class */
VideoSourceWinCallback::VideoSourceWinCallback(
    nsIOutputStream *pipe, int width, int height,
    nsIDOMCanvasRenderingContext2D *ctx)
{
    w = width;
    h = height;
    output = pipe;
    vCanvas = ctx;
    m_refCount = 0;
    
    /* Establish baseline stream time with absolute time since epoch */
    PRTime epoch_c = PR_Now();
    epoch = (PRFloat64)(epoch_c / MICROSECONDS);
    epoch += ((PRFloat64)(epoch_c % MICROSECONDS)) / MICROSECONDS;
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
    HRESULT hr;
    nsresult rv;
    
    PRUint32 wr;
    BYTE *pBuffer;
    long bufferLen;
    PRUint8 *i420buf;
    int start, end, fsize, isize, bpp;
    
    bpp = 4;
    fsize = w * h * 4;
    isize = w * h * 3 / 2;
    bufferLen = pSample->GetActualDataLength();
    
    hr = pSample->GetPointer(&pBuffer);
    nsAutoArrayPtr<PRUint8> rgb32(new PRUint8[bufferLen]);
    memcpy(rgb32.get(), pBuffer, bufferLen);
    pBuffer = rgb32.get();
    
    /* Reverse RGB32 top-down */
    PRUint8 tmp;
    for (start = 0, end = bufferLen - bpp;
            start < bufferLen / 2;
            start += bpp, end -= bpp) {

        /* While we're here let's do a RGB<->BGR swap */
        tmp = pBuffer[start];
        pBuffer[start] = pBuffer[end+2];
        pBuffer[end+2] = tmp;
        
        tmp = pBuffer[start+1];
        pBuffer[start+1] = pBuffer[end+1];
        pBuffer[end+1] = tmp;
        
        tmp = pBuffer[start+2];
        pBuffer[start+2] = pBuffer[end];
        pBuffer[end] = tmp;
        
        tmp = pBuffer[start+3];
        pBuffer[start+3] = pBuffer[end+3];
        pBuffer[end+3] = tmp;
    }
    
    /* Write sample to canvas if neccessary */
    if (vCanvas) {
        nsCOMPtr<nsIRunnable> render = new CanvasRenderer(
            vCanvas, w, h, rgb32, fsize
        );
        rv = NS_DispatchToMainThread(render);
    }
    
    /* Write header: timestamp + length */
    PRFloat64 current = epoch + Time;
    rv = output->Write(
        (const char *)&current, sizeof(PRFloat64), &wr
    );
    rv = output->Write(
        (const char *)&isize, sizeof(PRUint32), &wr
    );
    
    /* Write to pipe after converting to i420 */
    i420buf = (PRUint8 *)PR_Calloc(isize, sizeof(PRUint8));
    RGB32toI420(w, h, (const char *)pBuffer, (char *)i420buf);
    rv = output->Write((const char *)i420buf, isize, &wr);
    PR_Free(i420buf);
    
    return S_OK;
}

STDMETHODIMP
VideoSourceWinCallback::BufferCB(double Time, BYTE *pBuffer, long bufferLen)
{
    return E_NOTIMPL;
}
