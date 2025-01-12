include ../make.def

all: rcc

RCC_OBJS := rcc.o preproc.o parse.o compile.o rc_lexer.yy.o rc_parser.tab.o
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

rc_parser.y: $(OBJDIR)/rc_lexer.yy.h
preproc.c: $(OBJDIR)/rc_parser.tab.h

$(OBJDIR)/rc_parser.tab.c: rc_parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/rc_parser.tab.c rc_parser.y

$(OBJDIR)/rc_parser.tab.h: rc_parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/rc_parser.tab.c rc_parser.y

$(OBJDIR)/rc_lexer.yy.c: rc_lexer.l $(OBJDIR)/rc_parser.tab.h
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/rc_lexer.yy.h -o $(OBJDIR)/rc_lexer.yy.c rc_lexer.l

$(OBJDIR)/rc_lexer.yy.h: rc_lexer.l
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/rc_lexer.yy.h -o $(OBJDIR)/rc_lexer.yy.c rc_lexer.l

$(OBJDIR)/rc_parser.tab.o: $(OBJDIR)/rc_parser.tab.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/rc_lexer.yy.o: $(OBJDIR)/rc_lexer.yy.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f rcc $(RCC_OBJS) $(RCC_DEPS) $(OBJDIR)/*.tab.* $(OBJDIR)/*.yy.*
