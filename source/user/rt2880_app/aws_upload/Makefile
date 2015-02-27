EXEC = aws_upload
OBJS = aws_upload.o ../jansson-2.7/src/.libs/libjansson.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwinit.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwapp.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwxml.a ../w3c-libwww-5.4.0/modules/expat/xmlparse/.libs/libxmlparse.a ../w3c-libwww-5.4.0/modules/expat/xmltok/.libs/libxmltok.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwhtml.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwtelnet.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwnews.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwhttp.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwmime.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwgopher.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwftp.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwdir.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwcache.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwstream.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwfile.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwmux.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwtrans.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwcore.a ../w3c-libwww-5.4.0/Library/src/.libs/libwwwutils.a -lm ../w3c-libwww-5.4.0/modules/md5/.libs/libmd5.a
CFLAGS += -DHAVE_CONFIG_H -I../w3c-libwww-5.4.0/Library/Examples/ -I../w3c-libwww-5.4.0/Library/Examples/../.. -I../w3c-libwww-5.4.0/Library/Examples/../src  -I../w3c-libwww-5.4.0/Library/Examples/../../modules/expat/xmlparse  -I../w3c-libwww-5.4.0/Library/Examples/../src/SSL -I../w3c-libwww-5.4.0/Library/Examples/../../../jansson-2.7/src/  -g -O2 -Wall -c
LDFLAGS += -ldl -lm

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

