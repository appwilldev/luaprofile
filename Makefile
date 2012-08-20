CC=gcc
CFLAGS=-g -Wall -fPIC -O2 $(DEFINE)
LDFLAGS=-ldl
INSTALL=install
PREFIX=/usr/local

.PHONY: all clean

all: luaprofile.so luaprofile

luaprofile.so: luaprofile.o
	$(CC) -shared $< -o $@ $(LDFLAGS)

luaprofile: main.o
	$(CC) $< -o $@

clean:
	-rm *.o

install:
	$(INSTALL) -m755 -t $(PREFIX)/lib luaprofile.so
	$(INSTALL) -m755 -t $(PREFIX)/bin luaprofile
	ldconfig

uninstall:
	rm -f $(PREFIX)/lib/luaprofile.so
	rm -f $(PREFIX)/bin/luaprofile
	-ldconfig
