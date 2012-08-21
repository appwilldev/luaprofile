This is a simple library (and a convenient executable) to profile Lua functions.

Usage
=====

Compile and install it by running
```sh
make
sudo make install
```

You can also set `PREFIX=xxx` to install to a different location.

```
usage: luaprofile -l LUA_LIBRARY [-o LOGFILE] [-L PRELOAD_LIB] COMMAND ...

      -l, --lualib              The Lua library
      -o, --logfile             The log file, if not stderr
      -L, --preloadlib          The 'luaprofile.so' library, if not at default location

environment variables 'LUAP_LIBRARY', 'LUAP_LOGFILE' also work.
```

Sending a `SIGPROF` signal will make it print stats on next Lua function call.

Note
====
* Lua must be loaded as a dynamic library, not statically linked into the binary you will run.
* Works on Linux. Might also work on Mac OS X / FreeBSD.
* May not work if running as root.
