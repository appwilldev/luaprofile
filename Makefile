CC=gcc
CFLAGS=-g -Wall -fPIC -O2 $(DEFINE)
LDFLAGS=-ldl

.PHONY: all clean

all: luaprofile.so

luaprofile.so: luaprofile.o
	$(CC) -shared $< -o $@ $(LDFLAGS)

clean:
	-rm *.o
