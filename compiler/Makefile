include make.def

all: bindir asm rcc

bindir:
	mkdir -p $(TOPDIR)/bin

.PHONY: asm
asm: bindir
	make -C $(TOPDIR)/asm all
	cp $(TOPDIR)/asm/as $(TOPDIR)/bin/as
	cp $(TOPDIR)/asm/disas $(TOPDIR)/bin/disas

.PHONY: rcc
rcc: bindir
	make -C $(TOPDIR)/rcc all
	cp $(TOPDIR)/rcc/rcc $(TOPDIR)/bin/rcc

test:
	make -C $(TOPDIR)/asm test

.PHONY: clean
clean:
	make -C $(TOPDIR)/common clean
	make -C $(TOPDIR)/asm clean
	make -C $(TOPDIR)/rcc clean
	rm -f $(TOPDIR)/bin/as $(TOPDIR)/bin/disas $(TOPDIR)/bin/rcc
