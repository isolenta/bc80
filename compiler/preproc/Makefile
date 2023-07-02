include ../make.def

all: cpp

CPP_OBJS := cpp.o
CPP_OBJS := $(addprefix $(OBJDIR)/, $(CPP_OBJS))
CPP_DEPS := $(addprefix $(OBJDIR)/, $(CPP_DEPS:%.o=%.d))

-include $(CPP_DEPS)

CFLAGS += -I$(TOPDIR)/cpp -I$(TOPDIR)/cpp/$(OBJDIR)

$(TOPDIR)/common/libcommon.a:
	make -C $(TOPDIR)/common

cpp: $(TOPDIR)/common/libcommon.a $(CPP_OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

$(OBJDIR)/%.o: %.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.d: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MM $^ -MF $@

.PRECIOUS: $(OBJDIR)/%.d

clean:
	make -C $(TOPDIR)/common clean
	rm -f cpp $(CPP_OBJS) $(CPP_DEPS)
