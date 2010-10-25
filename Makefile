ifeq ($(TOPSRCDIR),)
  export TOPSRCDIR = $(shell pwd)
endif
ifeq ($(VERSION),)
  export VERSION = 0.1
endif

so_file=components/media/libmediarecorder.dylib
xpi_name=rainbow-$(VERSION)-dev.xpi
xpi_files=chrome.manifest install.rdf content/ $(so_file)

all: xpi

$(so_file):
	$(MAKE) -C components/media

xpi: $(so_file)
	rm -f $(TOPSRCDIR)/$(xpi_name)
	cd $(TOPSRCDIR);zip -9r $(xpi_name) $(xpi_files)

clean:
	rm -f $(TOPSRCDIR)/$(xpi_name)
	$(MAKE) -C components/media clean
