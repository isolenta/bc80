include ../make.def

all: libcommon.a

OBJS := common.o dynarray.o hashmap.o buffer.o filesystem.o mmgr.o
OBJS := $(addprefix $(OBJDIR)/, $(OBJS))
DEPS := $(OBJS:%.o=%.d)

-include $(DEPS)

libcommon.a: $(OBJS)
	ar -rc $@ $^

$(OBJDIR)/%.o: %.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.d: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MM $^ -MF $@

.PRECIOUS: $(OBJDIR)/%.d

clean:
	rm -f libcommon.a $(OBJS) $(DEPS)
