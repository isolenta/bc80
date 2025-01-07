include ../make.def

all: libcommon.a

OBJS := common.o dynarray.o hashmap.o buffer.o filesystem.o mmgr.o error.o
OBJS := $(addprefix $(OBJDIR)/, $(OBJS))
DEPS := $(OBJS:%.o=%.d)

-include $(DEPS)

CFLAGS += -MMD

libcommon.a: $(OBJS)
	ar -rc $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

.PHONY: clean
clean:
	rm -f libcommon.a $(OBJS) $(DEPS)
