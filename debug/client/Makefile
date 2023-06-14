all: debug

OBJS := main.o dynarray.o commands.o rl_utils.o
DEPS := $(OBJS:%.o=%.d)

BUILDDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CFLAGS := -g -ggdb -O2 -std=c11 -I$(BUILDDIR) -I$(BUILDDIR)/.. -I/opt/homebrew/opt/readline/include
LDFLAGS := -L/opt/homebrew/opt/readline/lib -lreadline

debug: $(OBJS)
	gcc $(LDFLAGS) -o $@ $^

-include $(DEPS)

%.o: src/%.c src/%.d
	gcc $(CFLAGS) -c $< -o $@

%.d: %.c
	gcc $(CFLAGS) -MM $^ -MF $@

clean:
	rm -f debug $(OBJS) $(DEPS)
