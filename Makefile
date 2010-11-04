ifeq ($(TOPSRCDIR),)
  export TOPSRCDIR = $(shell pwd)
endif
ifeq ($(VERSION),)
  export VERSION = 0.1
endif

sys := $(shell uname -s)
ifeq ($(sys), Darwin)
  so = dylib
else
ifeq ($(sys), MINGW32_NT-6.1)
  so = dll
else
ifeq ($(sys), Linux)
  so = so
else
  $(error Sorry, your os is unknown/unsupported: $(sys))
endif
endif
endif

so_files=components/libmediarecorder.$(so)
xpt_files=components/IMediaRecorder.xpt

xpi_name=rainbow-$(VERSION)-dev.xpi
xpi_files=chrome.manifest install.rdf content/ $(so_files) $(xpt_files)

all: xpi

$(so_files) $(xpt_files):
	$(MAKE) -C components

xpi: $(so_files) $(xpt_files)
	rm -f $(TOPSRCDIR)/$(xpi_name)
	cd $(TOPSRCDIR);zip -9r $(xpi_name) $(xpi_files)

clean:
	rm -f $(TOPSRCDIR)/$(xpi_name)
	$(MAKE) -C components clean

