CC=gcc
CFLAGS=-g -Wall -fPIC -O2 $(DEFINE)
LDFLAGS=-ldl
INSTALL=install

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
