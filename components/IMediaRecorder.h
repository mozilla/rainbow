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


/* starting interface:    IMediaRecorder */
#define IMEDIARECORDER_IID_STR "c467b1f4-551c-4e2f-a6ba-cb7d792d1452"

#define IMEDIARECORDER_IID \
  {0xc467b1f4, 0x551c, 0x4e2f, \
    { 0xa6, 0xba, 0xcb, 0x7d, 0x79, 0x2d, 0x14, 0x52 }}

class NS_NO_VTABLE NS_SCRIPTABLE IMediaRecorder : public nsISupports {
 public:

  NS_DECLARE_STATIC_IID_ACCESSOR(IMEDIARECORDER_IID)

  /* void beginSession (in nsIPropertyBag2 prop, in nsIDOMCanvasRenderingContext2D ctx, in nsIMediaStateObserver obs); */
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIMediaStateObserver *obs) = 0;

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
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIMediaStateObserver *obs); \
  NS_SCRIPTABLE NS_IMETHOD EndSession(void); \
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file); \
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void);

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_IMEDIARECORDER(_to) \
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIMediaStateObserver *obs) { return _to BeginSession(prop, ctx, obs); } \
  NS_SCRIPTABLE NS_IMETHOD EndSession(void) { return _to EndSession(); } \
  NS_SCRIPTABLE NS_IMETHOD BeginRecording(nsILocalFile *file) { return _to BeginRecording(file); } \
  NS_SCRIPTABLE NS_IMETHOD EndRecording(void) { return _to EndRecording(); }

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_IMEDIARECORDER(_to) \
  NS_SCRIPTABLE NS_IMETHOD BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIMediaStateObserver *obs) { return !_to ? NS_ERROR_NULL_POINTER : _to->BeginSession(prop, ctx, obs); } \
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

/* void beginSession (in nsIPropertyBag2 prop, in nsIDOMCanvasRenderingContext2D ctx, in nsIMediaStateObserver obs); */
NS_IMETHODIMP _MYCLASS_::BeginSession(nsIPropertyBag2 *prop, nsIDOMCanvasRenderingContext2D *ctx, nsIMediaStateObserver *obs)
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
