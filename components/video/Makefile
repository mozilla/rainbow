# OS
sys := $(shell uname -s)

ifeq ($(sys), Darwin)
  os = Darwin
  compiler = gcc
  cxx = g++
  so = dylib
  cppflags += -dynamiclib -DDEBUG -DMOZ_NO_MOZALLOC
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
  $(error: Sorry, your architecture is unknown/unsupported: $(machine))
endif
endif
endif

# Target and objects
target = libjetpackvideo
so_target = $(target:=.$(so))
cpp_objects = $(cpp_sources:.cpp=.o)

# source and path configurations
idl = IVideoRecorder.idl
cpp_sources = VideoRecorder.cpp VideoModule.cpp

sdkdir ?= ${MOZSDKDIR}
xpidl = $(sdkdir)/bin/xpidl

# FIXME: we don't actually require this for e.g. clean
ifeq ($(sdkdir),)
  $(warning No 'sdkdir' variable given)
  $(warning It should point to the location of the Gecko SDK)
  $(warning For example: "make sdkdir=/foo/bar/baz")
  $(warning Or set the MOZSDKDIR environment variable to point to it)
  $(error )
endif

idl_headers = $(idl:.idl=.h)
idl_typelib = $(idl:.idl=.xpt)
cpp_objects = $(cpp_sources:.cpp=.o)
so_target = $(target:=.$(so))

headers = -I. \
          -I$(sdkdir)/include \
          -I$(sdkdir)/include/system_wrappers \
          -I$(sdkdir)/include/xpcom \
          -I$(sdkdir)/include/string \
		  -I$(sdkdir)/include/content \
		  -I$(sdkdir)/include/dom \
          -I$(sdkdir)/include/nspr \
          -I$(sdkdir)/sdk/include 

# libraries
libdirs := $(sdkdir)/lib $(sdkdir)/bin
libs := xpcomglue_s xpcom nspr4 plds4 plc4

# compiler and Linker Flags
ifeq ($(os), Darwin)
  libdirs := $(patsubst %,-L%,$(libdirs))
  libs := $(patsubst %,-l%,$(libs))
  headers += -I../../lib/darwin_i386/include
  cppflags += -c -pipe -Os -arch i386 \
              -fPIC -fno-rtti -fno-exceptions -fno-strict-aliasing \
              -fno-common -fshort-wchar -fpascal-strings -pthread \
              -Wall -Wconversion -Wpointer-arith -Woverloaded-virtual \
			  -Wsynth -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor \
			  -Wcast-align -Wno-long-long \
              -include xpcom-config.h $(headers)
  ldflags += -pthread -pipe -bundle -arch i386 \
              -Wl,-executable_path,$(sdkdir)/bin \
              -Wl,-dead_strip \
              ../../lib/darwin_i386/libvidcap.a \
              ../../lib/darwin_i386/libogg.a \
              ../../lib/darwin_i386/libtheora.a \
              ../../lib/darwin_i386/libtheoraenc.a \
              -framework Carbon \
              -framework QuartzCore \
              -framework QuickTime \
              $(libdirs) $(libs)
endif

######################################################################

.PHONY: all build clean

all: build

build: $(so_target) $(idl_typelib)

clean: 
	rm -f $(so_target) $(cpp_objects) \
  $(idl_typelib) $(idl_headers) \
	$(target:=.res) fake.lib fake.exp

# rules to build the c headers and .xpt from idl
$(idl_headers): $(idl)
	$(xpidl) -m header -I$(sdkdir)/idl $(@:.h=.idl)

$(idl_typelib): $(idl)
	$(xpidl) -m typelib -I$(sdkdir)/idl $(@:.xpt=.idl)


ifeq ($(os), Darwin)
  $(cpp_objects): $(cpp_sources)
	$(cxx) -o $@ $(cppflags) $(@:.o=.cpp)

  $(so_target): $(idl_headers) $(cpp_objects)
	$(cxx) -o $@ $(ldflags) $(cpp_objects)
	chmod +x $@
endif
