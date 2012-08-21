#include<stdarg.h>
#include<dlfcn.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<sys/time.h>

#include<uthash.h>
#include<lua.h>

#define FILENAME_LEN 50
#define FUNCNAME_LEN 25
#define KEY_LEN (FILENAME_LEN + 1 + FUNCNAME_LEN + 1 + sizeof(int))

struct funccall {
    char key[KEY_LEN];
    char state;
    int c_count;
    int r_count;
    long time;
    long last_time; 
    UT_hash_handle hh;
};

static void lib_init();
static struct funccall *funccalls = NULL;
static int lib_initialized = 0;
static int sig_print = 0;
static FILE *logfile = NULL;
static lua_State* (*orig_luaL_newstate) (void) = 0;
static void       (*orig_lua_close) (lua_State *L) = 0;

static void hookfunc(lua_State *L, lua_Debug *ar);
static void printStats();
static char* generate_key(lua_Debug *ar);
static inline void copynright(char* dest, const char* src, int n);
static int cmp_funccall(struct funccall *a, struct funccall *b);
static void onsignal(int signo);

static void die(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  fflush(stderr);
  exit(-1);
}

#ifdef DEBUG
static void printkey(const char* key, int len){
  int i;
  for(i=0; i<len; i++){
    fprintf(logfile, "%02x", key[i]);
  }
  fprintf(logfile, "\n");
}
#endif

lua_State* luaL_newstate(void){
  lib_init();
  lua_State *L = orig_luaL_newstate();
  lua_sethook(L, hookfunc, LUA_MASKCALL|LUA_MASKRET, 0);
  fprintf(logfile, ":: Lua profiling library loaded.\n");
  fflush(logfile);
  return L;
}

static int cmp_funccall(struct funccall *a, struct funccall *b){
    return a->time - b->time;
}

void lua_close(lua_State *L){
  lib_init();
  orig_lua_close(L);
  fprintf(logfile, ":: Lua closed, stats follow.\n");
  printStats();
  fflush(logfile);
}

static void onsignal(int signo){
  sig_print = 1;
}

static void printStats(){
  struct funccall *fc, *tmp;
  char *key, *func;
  char buf[20];
  int lineno;
  int fd = fileno(logfile);
  double tottime, percall;

  HASH_SORT(funccalls, cmp_funccall);
  lockf(fd, F_LOCK, 0);
  fprintf(logfile, "============================== Stats (pid %5d) ==============================\n", getpid());
  fprintf(logfile, "  ncalls   tottime  percall file:lineno(function)\n");
  HASH_ITER(hh, funccalls, fc, tmp){
    key = fc->key;
    lineno = *(int*)(key + FILENAME_LEN + 2 + FUNCNAME_LEN);
    func = key + FILENAME_LEN + 1;
    tottime = fc->time / (double) 1000;
    percall = tottime / fc->c_count;
    if(fc->c_count == fc->r_count){
      fprintf(logfile, "%8d %9.3lf %8.3lf %s:%d(%s)\n",
	  fc->c_count, tottime, percall, key, lineno, func);
    }else{
      snprintf(buf, 20, "%d/%d", fc->c_count, fc->r_count);
      fprintf(logfile, "%8s %9.3lf %8.3lf %s:%d(%s)\n",
	  buf, tottime, percall, key, lineno, func);
    }
    if(!sig_print){
      HASH_DEL(funccalls, fc);
      free(fc);
    }
  }
  fprintf(logfile, "============================ Stats End (pid %5d) ============================\n", getpid());
  fflush(logfile);
  lockf(fd, F_ULOCK, 0);
}

static inline void copynright(char* dest, const char* src, int n){
  char* p = (char*)((strlen(src) > n) ? src + strlen(src) - n : src);
#ifdef DEBUG
  fprintf(logfile, "copying %s (at most %d)\n", src, n);
#endif
  strncpy(dest, p, n);
}

static char* generate_key(lua_Debug *ar){
  char* key = malloc(sizeof(char) * KEY_LEN);
  char* p = key;
  memset(p, 0, KEY_LEN);
  copynright(p, ar->short_src, FILENAME_LEN);

  p += FILENAME_LEN;
  *p = '\0';
  p++;

  char* name;
  if(!(ar->name)){
    int len = strlen(ar->what);
    name = malloc(sizeof(char) * (len + 3));
    strcpy(name + 1, ar->what);
    name[0] = '(';
    name[len+1] = ')';
    name[len+2] = '\0';
  }else{
    name = strdup(ar->name);
  }
  copynright(p, name, FUNCNAME_LEN);
  free(name);
  p += FUNCNAME_LEN;
  *p = '\0';
  p++;
  memcpy(p, &(ar->linedefined), sizeof(int));
  return key;
}

static void hookfunc(lua_State *L, lua_Debug *ar){
  char *event;
  char state;
  struct funccall *fc;
  struct timeval thistime;
  long t;
  lua_getinfo(L, "nS", ar);
  if(strcmp(ar->what, "C") == 0){
    if(sig_print){
      printStats();
      sig_print = 0;
    }
    return;
  }
  gettimeofday(&thistime, NULL);
  if(sig_print){
    printStats();
    sig_print = 0;
  }

  t = thistime.tv_sec * 1000 * 1000 + thistime.tv_usec;

  char* key = generate_key(ar);
  HASH_FIND(hh, funccalls, key, KEY_LEN, fc);
  if(fc){
#ifdef DEBUG
    fprintf(logfile, "Found an item with key ");
    printkey(key, KEY_LEN);
#endif
    free(key);
    key = fc->key;
  }else{
    fc = malloc(sizeof(struct funccall));
    fc->c_count = 0;
    fc->r_count = 0;
    fc->time = 0;
    memcpy(fc->key, key, KEY_LEN);
    free(key);
    key = fc->key;
#ifdef DEBUG
    printkey(key, KEY_LEN);
#endif
    HASH_ADD_KEYPTR(hh, funccalls, key, KEY_LEN, fc);
  }

  switch(ar->event){
    case LUA_HOOKCALL:
      fc->c_count++;
      state = 'c';
      event = "call";
      break;
    case LUA_HOOKTAILRET:
#ifdef DEBUG
      fprintf(logfile, "Tail call return!\n");
#endif
    case LUA_HOOKRET:
      fc->r_count++;
      state = 'r';
      event = "return";
      break;
    default:
      state = 'X'; /* make gcc happy */
      die("Shouldn't reach here!");
  }
  if(state == 'r' && fc->state == 'c'){
      fc->time += t - fc->last_time;
  }
  fc->last_time = t;
  fc->state = state;

#ifdef DEBUG
  fprintf(logfile, "%4s: [%s:%d] func %s %s\n", ar->what, key, *(int*)(key + FILENAME_LEN + 2 + FUNCNAME_LEN),
      key + FILENAME_LEN + 1, event);
#endif
}

static void lib_init() {
  void *libhdl;
  char *dlerr;
  char *path;

  if (lib_initialized) return;

  path = getenv("LUAP_LIBRARY");
  if(!path){
    die("Please point LUAP_LIBRARY to your lua executable or library.");
  }

  if (!(libhdl=dlopen(path, RTLD_LAZY)))
    die("Failed to patch library calls: %s", dlerror());

  orig_luaL_newstate = dlsym(libhdl, "luaL_newstate");
  if((dlerr=dlerror()) != NULL)
    die("Failed to patch luaL_newstate() library call: %s", dlerr);

  orig_lua_close = dlsym(libhdl, "lua_close");
  if((dlerr=dlerror()) != NULL)
    die("Failed to patch lua_close() library call: %s", dlerr);

  path = getenv("LUAP_LOGFILE");
  logfile = fopen(path, "a");
  if(!logfile)
    logfile = stderr;

  struct sigaction act, oact;
  memset(&act, 0, sizeof(act));
  act.sa_handler = onsignal;
  sigaction(SIGPROF, &act, &oact);

  lib_initialized = 1;
}
