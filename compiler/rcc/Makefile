include ../make.def

all: rcc

RCC_OBJS := rcc.o preproc.o parse.o compile.o
RCC_OBJS := $(addprefix $(OBJDIR)/, $(RCC_OBJS))
RCC_DEPS := $(RCC_OBJS:%.o=%.d)

-include $(RCC_DEPS)

CFLAGS += -I$(TOPDIR)/rcc/$(OBJDIR) -MMD

common:
	make -C $(TOPDIR)/common

libasm:
	make -C $(TOPDIR)/asm

rcc: common libasm $(RCC_OBJS)
	$(CC) -o $@ $(LDFLAGS) $(TOPDIR)/common/libcommon.a $(TOPDIR)/asm/libasm.a $(RCC_OBJS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f rcc $(RCC_OBJS) $(RCC_DEPS)
