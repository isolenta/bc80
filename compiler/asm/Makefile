include ../make.def

all: libasm.a as disas

LIB_OBJS := libasm.o parse.o compile.o render.o instruction.o lexer.yy.o parser.tab.o disas_opcodes.o disas_print.o
LIB_OBJS := $(addprefix $(OBJDIR)/, $(LIB_OBJS))
LIB_DEPS := $(addprefix $(OBJDIR)/, $(LIB_OBJS:%.o=%.d))

AS_OBJS := as.o
AS_OBJS := $(addprefix $(OBJDIR)/, $(AS_OBJS))
AS_DEPS := $(addprefix $(OBJDIR)/, $(AS_DEPS:%.o=%.d))

DISAS_OBJS := disas.o
DISAS_OBJS := $(addprefix $(OBJDIR)/, $(DISAS_OBJS))
DISAS_DEPS := $(addprefix $(OBJDIR)/, $(DISAS_DEPS:%.o=%.d))

-include $(LIB_DEPS) $(AS_DEPS) $(DISAS_DEPS)

CFLAGS += -I$(TOPDIR)/asm -I$(TOPDIR)/asm/$(OBJDIR)

libasm.a: $(LIB_OBJS)
	ar -rc $@ $^

$(TOPDIR)/common/libcommon.a:
	make -C $(TOPDIR)/common

as: $(TOPDIR)/common/libcommon.a $(AS_OBJS) libasm.a
	$(CC) -o $@ $(LDFLAGS) $^

disas: $(TOPDIR)/common/libcommon.a $(DISAS_OBJS) libasm.a
	$(CC) -o $@ $(LDFLAGS) $^

parser.y: $(OBJDIR)/lexer.yy.h
parse.c: $(OBJDIR)/parser.tab.h

$(OBJDIR)/parser.tab.c: parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c parser.y

$(OBJDIR)/parser.tab.h: parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c parser.y

$(OBJDIR)/lexer.yy.c: lexer.l
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

$(OBJDIR)/%.o: %.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.d: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MM $^ -MF $@

.PRECIOUS: $(OBJDIR)/%.d

test: as disas
	@$(TOPDIR)/asm/test.sh $(TEST)

clean:
	make -C $(TOPDIR)/common clean
	rm -f as disas libasm.a $(LIB_OBJS) $(LIB_DEPS) $(AS_OBJS) $(AS_DEPS) $(DISAS_OBJS) $(DISAS_DEPS) $(OBJDIR)/*.tab.* $(OBJDIR)/*.yy.*
