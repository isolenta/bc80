BUILDDIR := _build

.PHONY: all
all: build

.PHONY: build
build: $(BUILDDIR)
	cmake --build $(BUILDDIR) --config Debug
	cmake --install $(BUILDDIR) --config Debug

$(BUILDDIR):
	cmake -B$(BUILDDIR)

test: $(BUILDDIR)
	cmake --build _build --target test

.PHONY: clean
clean: $(BUILDDIR)
	cmake --build $(BUILDDIR) --target clean
	rm -rf $(BUILDDIR)
