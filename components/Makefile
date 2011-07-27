# OS
sys := $(shell uname -s)

ifeq ($(sys), Linux)
  os = Linux
  compiler = gcc
  cxx = g++
  so = so
else
  $(error Sorry, your os is unknown/unsupported: $(sys))
endif

# Arch
machine := $(shell uname -m)

ifeq ($(machine), i386)
  arch = x86
else
ifeq ($(machine), i586)
  arch = x86
else
ifeq ($(machine), i686)
  arch = x86
else
ifeq ($(machine), x86_64)
  arch = x86
else
  $(error: Sorry, your architecture is unknown/unsupported: $(machine))
endif
endif
endif
endif

# Target and objects
target = libmediarecorder
so_target = $(target:=.$(so))

# source and path configurations
idl = IMediaRecorder.idl
cpp_sources = MediaRecorder.cpp MediaModule.cpp Convert.cpp \
    VideoSource.cpp AudioSource.cpp $(os)/AudioSourceNix.cpp \
    VideoSourceGIPS.cpp VideoSourceCanvas.cpp 

sdkdir ?= ${MOZSDKDIR}
rtcdir ?= ${WEBRTCDIR}

# FIXME: we don't actually require this for e.g. clean
ifeq ($(sdkdir),)
  $(warning No 'sdkdir' variable given)
  $(warning It should point to the location of the Gecko SDK)
  $(warning For example: "make sdkdir=/foo/bar/baz")
  $(warning Or set the MOZSDKDIR environment variable to point to it)
  $(error )
endif
ifeq ($(rtcdir),)
  $(warning No 'rtcdir' variable given)
  $(warning It should point to the location of the WebRTC trunk)
  $(warning For example: "make rtcdir=/foo/bar/baz")
  $(warning Or set the WEBRTCDIR environment variable to point to it)
  $(error )
endif
xpidl = $(sdkdir)/bin/xpidl

idl_headers = $(idl:.idl=.h)
idl_typelib = $(idl:.idl=.xpt)
cpp_objects = $(cpp_sources:.cpp=.o)
so_target = $(target:=.$(so))

headers = -I. -I$(os) -I$(sdkdir)/include \
    -I$(sdkdir)/include/system_wrappers \
    -I$(sdkdir)/include/xpcom \
    -I$(sdkdir)/include/string \
    -I$(sdkdir)/include/content \
    -I$(sdkdir)/include/dom \
    -I$(sdkdir)/include/nspr \
    -I$(sdkdir)/sdk/include

# libraries
libdirs := $(sdkdir)/lib $(sdkdir)/bin
libs := xpcomglue_s xpcom nspr4 mozalloc plds4 plc4

# compiler and Linker Flags
ifeq ($(os), Linux)
  rtcprefix := $(rtcdir)/out/Debug/obj.target
  libdirs := $(patsubst %,-L%,$(libdirs))
  libs := $(patsubst %,-l%,$(libs))
  headers += -I../lib/Linux_x86-gcc3/include \
    -I$(rtcdir)/src \
    -I$(rtcdir)/src/video_engine/main/interface \
    -I../lib/$(os)_$(machine)/include

  ifeq ($(debug),1)
    cppflags += -g -DDEBUG
  endif

  cppflags += -pipe -Os -shared \
    -DMOZ_NO_MOZALLOC -DMOZILLA_STRICT_API -fshort-wchar \
    -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
    -fno-common -pthread -Wconversion -std=gnu++0x \
    -Wpointer-arith -Woverloaded-virtual -Wsynth \
    -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor -Wcast-align \
    -Wno-long-long -include xpcom-config.h $(headers)

  ldflags += -pthread -pipe \
    -Wl,-z,defs \
    -Wl,-rpath-link,$(sdkdir)/bin \
    $(sdkdir)/lib/libxpcomglue_s.a \
    $(libdirs) $(libs) -lrt -lasound \
    -lXext -lX11 -lpulse -ldl \
    ../lib/$(os)_$(machine)/libvorbis.a \
    ../lib/$(os)_$(machine)/libvorbisenc.a \
    ../lib/$(os)_$(machine)/libogg.a \
    ../lib/$(os)_$(machine)/libtheora.a

  rtclibs := \
    src/video_engine/main/source/libvideo_engine_core.a \
    src/voice_engine/main/source/libvoice_engine_core.a \
    src/modules/video_render/main/source/libvideo_render_module.a \
    src/modules/media_file/source/libmedia_file.a \
    src/modules/video_processing/main/source/libvideo_processing.a \
    src/modules/video_capture/main/source/libvideo_capture_module.a \
    src/modules/utility/source/libwebrtc_utility.a \
    src/modules/video_coding/main/source/libwebrtc_video_coding.a \
    src/modules/video_coding/codecs/vp8/main/source/libwebrtc_vp8.a \
    src/common_video/jpeg/main/source/libwebrtc_jpeg.a \
    third_party/libvpx/libvpx.a \
    third_party/libjpeg_turbo/libjpeg_turbo.a \
    src/modules/audio_coding/main/source/libaudio_coding_module.a \
    src/modules/audio_processing/main/source/libaudio_processing.a \
    src/modules/audio_device/main/source/libaudio_device.a \
    src/modules/audio_coding/NetEQ/main/source/libNetEq.a \
    src/modules/audio_processing/ns/main/source/libns.a \
    src/modules/audio_processing/aecm/main/source/libaecm.a \
    src/modules/audio_processing/aec/main/source/libaec.a \
    src/common_audio/resampler/main/source/libresampler.a \
    src/modules/audio_coding/codecs/iLBC/main/source/libiLBC.a \
    src/modules/audio_processing/agc/main/source/libagc.a \
    src/modules/audio_coding/codecs/CNG/main/source/libCNG.a \
    src/modules/audio_coding/codecs/iSAC/fix/source/libiSACFix.a \
    src/modules/audio_coding/codecs/iSAC/main/source/libiSAC.a \
    src/common_audio/vad/main/source/libvad.a \
    src/modules/udp_transport/source/libudp_transport.a \
    src/modules/audio_conference_mixer/source/libaudio_conference_mixer.a \
    src/modules/rtp_rtcp/source/librtp_rtcp.a \
    src/modules/video_coding/codecs/i420/main/source/libwebrtc_i420.a \
    src/common_video/vplib/main/source/libwebrtc_vplib.a \
    src/modules/audio_coding/codecs/G711/main/source/libG711.a \
    src/modules/audio_processing/utility/libapm_util.a \
    src/common_audio/signal_processing_library/main/source/libspl.a \
    src/modules/audio_coding/codecs/G722/main/source/libG722.a \
    src/modules/audio_coding/codecs/PCM16B/main/source/libPCM16B.a \
    src/system_wrappers/source/libsystem_wrappers.a

  ldflags += $(patsubst %,$(rtcprefix)/%,$(rtclibs))
endif

######################################################################

.PHONY: all build clean

all: build

build: $(so_target) $(idl_typelib)

clean:
	rm -f $(so_target) $(so_target).manifest $(cpp_objects) \
	$(target:=.res) libmediarecorder.lib libmediarecorder.exp \
	IMediaRecorder.h IMediaRecorder.xpt $(target).pdb $(target).ilk

# rules to build the c headers and .xpt from idl
$(idl_headers): $(idl)
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)

$(idl_typelib): $(idl)
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)

ifeq ($(os), Linux)
  $(so_target): $(idl_headers)
	$(cxx) $(cppflags) -o $@ $(cpp_sources) $(ldflags)
	chmod +x $@
endif
