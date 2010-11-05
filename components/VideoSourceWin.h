/*
 * Video source implementation for Windows.
 * We use the deprecated ISampleGrabber interface,
 * and thus we include parts of qedit.h here.
 */

#include "VideoSource.h"
#include <windows.h>
#include <dshow.h>

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
    VideoSourceWin(int n, int d, int w, int h);
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

    VideoSourceWinCallback *cb;

};
