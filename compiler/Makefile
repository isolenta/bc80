include make.def

all: $(TOPDIR)/as $(TOPDIR)/disas $(TOPDIR)/cpp

$(TOPDIR)/as $(TOPDIR)/disas:
	make -C $(TOPDIR)/asm all
	cp $(TOPDIR)/asm/as $(TOPDIR)/as
	cp $(TOPDIR)/asm/disas $(TOPDIR)/disas

$(TOPDIR)/cpp:
	make -C $(TOPDIR)/preproc all
	cp $(TOPDIR)/preproc/cpp $(TOPDIR)/cpp

test:
	make -C $(TOPDIR)/asm test

clean:
	make -C $(TOPDIR)/common clean
	make -C $(TOPDIR)/asm clean
	make -C $(TOPDIR)/preproc clean
	rm -f $(TOPDIR)/as $(TOPDIR)/disas $(TOPDIR)/cpp
