ifeq ($(TOPSRCDIR),)
  export TOPSRCDIR = $(shell pwd)
endif
ifeq ($(VERSION),)
  export VERSION = 0.1
endif

so_files=components/media/libmediarecorder.dylib 
xpt_files=components/media/IMediaRecorder.xpt

xpi_name=rainbow-$(VERSION)-dev.xpi
xpi_files=chrome.manifest install.rdf content/ $(so_files) $(xpt_files)

all: xpi

$(so_files) $(xpt_files):
	$(MAKE) -C components/media

xpi: $(so_files) $(xpt_files)
	rm -f $(TOPSRCDIR)/$(xpi_name)
	cd $(TOPSRCDIR);zip -9r $(xpi_name) $(xpi_files)

clean:
	rm -f $(TOPSRCDIR)/$(xpi_name)
	$(MAKE) -C components/media clean

