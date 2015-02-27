EXEC = iqomgr
OBJS = iqomgr.o ../jansson-2.7/src/.libs/libjansson.a
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/char/ -I../jansson-2.7/src/
LDFLAGS += -lm

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

