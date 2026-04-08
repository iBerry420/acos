VERSION ?= 2.0.0
ARCH ?= $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
UARCH ?= $(shell uname -m)
BINARY ?= ../build/avacli
OUTDIR ?= ../dist
REPO_ROOT ?= /var/www/packages.avalynn.ai

.PHONY: all deb tarball repo publish clean

all: deb tarball

deb: $(BINARY)
	bash build-deb.sh $(VERSION) $(ARCH) $(BINARY) $(OUTDIR)

tarball: $(BINARY)
	bash build-tarball.sh $(VERSION) $(UARCH) $(BINARY) $(OUTDIR)

repo:
	bash setup-apt-repo.sh $(REPO_ROOT)

publish:
	cp $(OUTDIR)/avacli_$(VERSION)_$(ARCH).deb $(REPO_ROOT)/pool/main/
	bash publish-repo.sh $(REPO_ROOT)

clean:
	rm -rf $(OUTDIR)/avacli*

$(BINARY):
	@echo "Binary not found at $(BINARY). Build first: cd .. && mkdir -p build && cd build && cmake .. && make -j$$(nproc)"
	@exit 1
