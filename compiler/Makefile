BISON := /opt/homebrew/opt/bison/bin/bison
FLEX := /opt/homebrew/opt/flex/bin/flex

all: libasm80.a as disas

OBJDIR := .obj

LIB_OBJS := libasm80.o common.o dynarray.o hashmap.o buffer.o parse.o compile.o render.o filesystem.o \
					instruction.o lexer.yy.o parser.tab.o disas_opcodes.o disas_print.o
LIB_OBJS := $(addprefix $(OBJDIR)/, $(LIB_OBJS))
LIB_DEPS := $(addprefix src/, $(AS_OBJS:%.o=%.d))

AS_OBJS := as.o
AS_DEPS := $(addprefix src/, $(AS_OBJS:%.o=%.d))

DISAS_OBJS := disas.o
DISAS_DEPS := $(addprefix src/, $(DISAS_OBJS:%.o=%.d))

BUILDDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

AS_OBJS := $(addprefix $(OBJDIR)/, $(AS_OBJS))
AS_DEPS := $(addprefix $(OBJDIR)/, $(AS_DEPS))
DISAS_OBJS := $(addprefix $(OBJDIR)/, $(DISAS_OBJS))
DISAS_DEPS := $(addprefix $(OBJDIR)/, $(DISAS_DEPS))

CFLAGS := -g -ggdb -O2 -fomit-frame-pointer -std=c11 \
					-I$(BUILDDIR) -I$(BUILDDIR)/src -I$(BUILDDIR)/$(OBJDIR)

test: as disas
	@$(shell pwd)/test.sh $(TEST)

libasm80.a: $(LIB_OBJS)
	ar -rc $@ $^

as: $(AS_OBJS) libasm80.a
	gcc -o $@ $^

disas: $(DISAS_OBJS) libasm80.a
	gcc -o $@ $^

-include $(AS_DEPS) $(DISAS_DEPS)

#src/lexer.l: $(OBJDIR)/parser.tab.h
src/parser.y: $(OBJDIR)/lexer.yy.h
src/parse.c: $(OBJDIR)/parser.tab.h

$(OBJDIR)/parser.tab.c: src/parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c src/parser.y

$(OBJDIR)/parser.tab.h: src/parser.y
	@mkdir -p $(dir $@)
	$(BISON) -v -d -o $(OBJDIR)/parser.tab.c src/parser.y

$(OBJDIR)/lexer.yy.c: src/lexer.l
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/lexer.yy.h -o $(OBJDIR)/lexer.yy.c src/lexer.l

$(OBJDIR)/lexer.yy.h: src/lexer.l
	@mkdir -p $(dir $@)
	$(FLEX) --header-file=$(OBJDIR)/lexer.yy.h -o $(OBJDIR)/lexer.yy.c src/lexer.l

$(OBJDIR)/parser.tab.o: $(OBJDIR)/parser.tab.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(OBJDIR)/lexer.yy.o: $(OBJDIR)/lexer.yy.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: src/%.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.d: src/%.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -MM $^ -MF $@

.PRECIOUS: $(OBJDIR)/%.d

clean:
	rm -f as disas libasm.a $(LIB_OBJS) $(AS_OBJS) $(AS_DEPS) $(DISAS_OBJS) $(DISAS_DEPS) $(OBJDIR)/*.tab.* $(OBJDIR)/*.yy.*
