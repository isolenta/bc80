include ../make.def

all: libasm as disas

LIB_OBJS := libasm.o parse.o compile.o render.o render_elf.o render_sna.o instruction.o lexer.yy.o parser.tab.o disas_opcodes.o disas_print.o
LIB_OBJS := $(addprefix $(OBJDIR)/, $(LIB_OBJS))
LIB_DEPS := $(LIB_OBJS:%.o=%.d)

AS_OBJS := as.o
AS_OBJS := $(addprefix $(OBJDIR)/, $(AS_OBJS))
AS_DEPS := $(AS_OBJS:%.o=%.d)

DISAS_OBJS := disas.o
DISAS_OBJS := $(addprefix $(OBJDIR)/, $(DISAS_OBJS))
DISAS_DEPS := $(DISAS_OBJS:%.o=%.d)

-include $(LIB_DEPS) $(AS_DEPS) $(DISAS_DEPS)

CFLAGS += -I$(TOPDIR)/asm/$(OBJDIR) -MMD

libasm: $(LIB_OBJS)
	ar -rc libasm.a $^

common:
	make -C $(TOPDIR)/common

as: common $(AS_OBJS) libasm
	$(CC) -o $@ $(LDFLAGS) $(TOPDIR)/common/libcommon.a $(AS_OBJS) libasm.a

disas: common $(DISAS_OBJS) libasm
	$(CC) -o $@ $(LDFLAGS) $(TOPDIR)/common/libcommon.a $(DISAS_OBJS) libasm.a

parser.y: $(OBJDIR)/lexer.yy.h
parse.c: $(OBJDIR)/parser.tab.h

$(OBJDIR)/parser.tab.c: parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c parser.y

$(OBJDIR)/parser.tab.h: parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c parser.y

$(OBJDIR)/lexer.yy.c: lexer.l $(OBJDIR)/parser.tab.h
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/lexer.yy.h -o $(OBJDIR)/lexer.yy.c lexer.l

$(OBJDIR)/lexer.yy.h: lexer.l
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/lexer.yy.h -o $(OBJDIR)/lexer.yy.c lexer.l

$(OBJDIR)/parser.tab.o: $(OBJDIR)/parser.tab.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/lexer.yy.o: $(OBJDIR)/lexer.yy.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: as disas
	@$(TOPDIR)/asm/test.sh $(TEST)

.PHONY: clean
clean:
	rm -f as disas libasm.a $(LIB_OBJS) $(LIB_DEPS) $(AS_OBJS) $(AS_DEPS) $(DISAS_OBJS) $(DISAS_DEPS) $(OBJDIR)/*.tab.* $(OBJDIR)/*.yy.*
