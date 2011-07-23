/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM IMediaRecorder.idl
 */

#ifndef __gen_IMediaRecorder_h__
#define __gen_IMediaRecorder_h__


#ifndef __gen_nsILocalFile_h__
#include "nsILocalFile.h"
#endif

#ifndef __gen_nsIOutputStream_h__
#include "nsIOutputStream.h"
#endif

#ifndef __gen_nsIPropertyBag2_h__
#include "nsIPropertyBag2.h"
#endif

#ifndef __gen_nsIDOMHTMLAudioElement_h__
#include "nsIDOMHTMLAudioElement.h"
#endif

#ifndef __gen_nsIDOMCanvasRenderingContext2D_h__
#include "nsIDOMCanvasRenderingContext2D.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    nsIMediaStateObserver */
#define NS_IMEDIASTATEOBSERVER_IID_STR "9d66a225-0134-4c6f-a6e2-466c899bdde4"

#define NS_IMEDIASTATEOBSERVER_IID \
  {0x9d66a225, 0x0134, 0x4c6f, \
    { 0xa6, 0xe2, 0x46, 0x6c, 0x89, 0x9b, 0xdd, 0xe4 }}

class NS_NO_VTABLE NS_SCRIPTABLE nsIMediaStateObserver : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMEDIASTATEOBSERVER_IID)

  /* void onStateChange (in string state, in string args); */
  NS_SCRIPTABLE NS_IMETHOD OnStateChange(const char *state, const char *args) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(nsIMediaStateObserver, NS_IMEDIASTATEOBSERVER_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIMEDIASTATEOBSERVER \
  NS_SCRIPTABLE NS_IMETHOD OnStateChange(const char *state, const char *args); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIMEDIASTATEOBSERVER(_to) \
  NS_SCRIPTABLE NS_IMETHOD OnStateChange(const char *state, const char *args) { return _to OnStateChange(state, args); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIMEDIASTATEOBSERVER(_to) \
  NS_SCRIPTABLE NS_IMETHOD OnStateChange(const char *state, const char *args) { return !_to ? NS_ERROR_NULL_POINTER : _to->OnStateChange(state, args); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsMediaStateObserver : public nsIMediaStateObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEDIASTATEOBSERVER

  nsMediaStateObserver();

private:
  ~nsMediaStateObserver();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsMediaStateObserver, nsIMediaStateObserver)

nsMediaStateObserver::nsMediaStateObserver()
{
  /* member initializers and constructor code */
}

nsMediaStateObserver::~nsMediaStateObserver()
{
  /* destructor code */
}

/* void onStateChange (in string state, in string args); */
NS_IMETHODIMP nsMediaStateObserver::OnStateChange(const char *state, const char *args)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    nsIAudioSample */
#define NS_IAUDIOSAMPLE_IID_STR "c4bb02e5-c1d7-4cde-90be-842424bfc1cc"

#define NS_IAUDIOSAMPLE_IID \
  {0xc4bb02e5, 0xc1d7, 0x4cde, \
    { 0x90, 0xbe, 0x84, 0x24, 0x24, 0xbf, 0xc1, 0xcc }}

class NS_NO_VTABLE NS_SCRIPTABLE nsIAudioSample : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IAUDIOSAMPLE_IID)

  /* attribute jsval frames; */
  NS_SCRIPTABLE NS_IMETHOD GetFrames(jsval *aFrames) = 0;
  NS_SCRIPTABLE NS_IMETHOD SetFrames(const jsval & aFrames) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(nsIAudioSample, NS_IAUDIOSAMPLE_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIAUDIOSAMPLE \
  NS_SCRIPTABLE NS_IMETHOD GetFrames(jsval *aFrames); \
  NS_SCRIPTABLE NS_IMETHOD SetFrames(const jsval & aFrames); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIAUDIOSAMPLE(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetFrames(jsval *aFrames) { return _to GetFrames(aFrames); } \
  NS_SCRIPTABLE NS_IMETHOD SetFrames(const jsval & aFrames) { return _to SetFrames(aFrames); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIAUDIOSAMPLE(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetFrames(jsval *aFrames) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetFrames(aFrames); } \
  NS_SCRIPTABLE NS_IMETHOD SetFrames(const jsval & aFrames) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetFrames(aFrames); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsAudioSample : public nsIAudioSample
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOSAMPLE

  nsAudioSample();

private:
  ~nsAudioSample();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAudioSample, nsIAudioSample)

nsAudioSample::nsAudioSample()
{
  /* member initializers and constructor code */
}

nsAudioSample::~nsAudioSample()
{
  /* destructor code */
}

/* attribute jsval frames; */
NS_IMETHODIMP nsAudioSample::GetFrames(jsval *aFrames)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsAudioSample::SetFrames(const jsval & aFrames)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    nsIAudioSampler */
#define NS_IAUDIOSAMPLER_IID_STR "84dc6b74-56e6-44f9-8435-0647f71e7c2a"

#define NS_IAUDIOSAMPLER_IID \
  {0x84dc6b74, 0x56e6, 0x44f9, \
    { 0x84, 0x35, 0x06, 0x47, 0xf7, 0x1e, 0x7c, 0x2a }}

class NS_NO_VTABLE NS_SCRIPTABLE nsIAudioSampler : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IAUDIOSAMPLER_IID)

  /* void onSampleReceived (in nsIAudioSample sample); */
  NS_SCRIPTABLE NS_IMETHOD OnSampleReceived(nsIAudioSample *sample) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(nsIAudioSampler, NS_IAUDIOSAMPLER_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIAUDIOSAMPLER \
  NS_SCRIPTABLE NS_IMETHOD OnSampleReceived(nsIAudioSample *sample); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIAUDIOSAMPLER(_to) \
  NS_SCRIPTABLE NS_IMETHOD OnSampleReceived(nsIAudioSample *sample) { return _to OnSampleReceived(sample); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIAUDIOSAMPLER(_to) \
  NS_SCRIPTABLE NS_IMETHOD OnSampleReceived(nsIAudioSample *sample) { return !_to ? NS_ERROR_NULL_POINTER : _to->OnSampleReceived(sample); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsAudioSampler : public nsIAudioSampler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOSAMPLER

  nsAudioSampler();

private:
  ~nsAudioSampler();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAudioSampler, nsIAudioSampler)

nsAudioSampler::nsAudioSampler()
{
  /* member initializers and constructor code */
}

nsAudioSampler::~nsAudioSampler()
{
  /* destructor code */
}

/* void onSampleReceived (in nsIAudioSample sample); */
NS_IMETHODIMP nsAudioSampler::OnSampleReceived(nsIAudioSample *sample)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


/* starting interface:    IMediaRecorder */
#define IMEDIARECORDER_IID_STR "c467b1f4-551c-4e2f-a6ba-cb7d792d1452"

#define IMEDIARECORDER_IID \
  {0xc467b1f4, 0x551c, 0x4e2f, \
    { 0xa6, 0xba, 0xcb, 0x7d, 0x79, 0x2d, 0x14, 0x52 }}

class NS_NO_VTABLE NS_SCRIPTABLE IMediaRecorder : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(IMEDIARECORDER_IID)

  /* [implicit_jscontext] void beginSession (in nsIPropertyBag2 prop, in nsIDOMCanvasRenderingContext2D ctx, in nsIDOMHTMLAudioElement audio, in nsIAudioSampler slr, in nsIMediaStateObserver obs); */
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIDOMHTMLAudioElement *audio, nsIAudioSampler *slr, nsIMediaStateObserver *obs, JSContext *cx) = 0;

  /* void endSession (); */
  NS_SCRIPTABLE NS_IMETHOD EndSession(void) = 0;

  /* void beginRecording (in nsILocalFile file); */
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file) = 0;

  /* void endRecording (); */
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(IMediaRecorder, IMEDIARECORDER_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_IMEDIARECORDER \
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIDOMHTMLAudioElement *audio, nsIAudioSampler *slr, nsIMediaStateObserver *obs, JSContext *cx); \
  NS_SCRIPTABLE NS_IMETHOD EndSession(void); \
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file); \
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_IMEDIARECORDER(_to) \
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIDOMHTMLAudioElement *audio, nsIAudioSampler *slr, nsIMediaStateObserver *obs, JSContext *cx) { return _to BeginSession(prop, ctx, audio, slr, obs, cx); } \
  NS_SCRIPTABLE NS_IMETHOD EndSession(void) { return _to EndSession(); } \
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file) { return _to BeginRecording(file); } \
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void) { return _to EndRecording(); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_IMEDIARECORDER(_to) \
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIDOMHTMLAudioElement *audio, nsIAudioSampler *slr, nsIMediaStateObserver *obs, JSContext *cx) { return !_to ? NS_ERROR_NULL_POINTER : _to->BeginSession(prop, ctx, audio, slr, obs, cx); } \
  NS_SCRIPTABLE NS_IMETHOD EndSession(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->EndSession(); } \
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file) { return !_to ? NS_ERROR_NULL_POINTER : _to->BeginRecording(file); } \
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->EndRecording(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public IMediaRecorder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMEDIARECORDER

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, IMediaRecorder)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* [implicit_jscontext] void beginSession (in nsIPropertyBag2 prop, in nsIDOMCanvasRenderingContext2D ctx, in nsIDOMHTMLAudioElement audio, in nsIAudioSampler slr, in nsIMediaStateObserver obs); */
NS_IMETHODIMP _MYCLASS_::BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIDOMHTMLAudioElement *audio, nsIAudioSampler *slr, nsIMediaStateObserver *obs, JSContext *cx)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endSession (); */
NS_IMETHODIMP _MYCLASS_::EndSession()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void beginRecording (in nsILocalFile file); */
NS_IMETHODIMP _MYCLASS_::BeginRecording(nsILocalFile *file)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endRecording (); */
NS_IMETHODIMP _MYCLASS_::EndRecording()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_IMediaRecorder_h__ */
