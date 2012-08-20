#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<getopt.h>

#define LIB_FILE "luaprofile.so"
#define PRELOAD_LEN 2048

void usage(const char* name){
  printf("usage: %s -f LOGFILE -l LUA_LIBRARY [-L PRELOAD_LIB] COMMAND ...\n",
      name);
}

int main(int argc, char **argv){
  static struct option long_options[] = {
    {"logfile",    required_argument, 0, 'f'},
    {"lualib",     required_argument, 0, 'l'},
    {"preloadlib", required_argument, 0, 'L'},
    {"help",       no_argument,       0, 'h'},
    {0,            0,                 0, 0 }
  };

  setenv("POSIXLY_CORRECT", "1", 0);
  int n = 0;
  char c;
  char *preload = NULL;
  char *preload_env;
  char *old_preload = getenv("LD_PRELOAD");

  while(1){
    c = getopt_long(argc, argv, "hf:l:L:", long_options, &n);
    if(c == -1)
      break;

    switch(c){
      case 'f':
	setenv("LUAP_LOGFILE", optarg, 0);
	printf("logfile is %s.\n", optarg);
	break;
      case 'l':
	printf("library is %s.\n", optarg);
	setenv("LUAP_LIBRARY", optarg, 0);
	break;
      case 'L':
	preload = optarg;
	break;
      case 'h':
	usage(argv[0]);
	return 0;
      case '?':
	return 1;
      default:
	fprintf(stderr, "shouldn't reach here.\n");
	return 1;
    }
  }
  unsetenv("POSIXLY_CORRECT");
  preload_env = malloc(PRELOAD_LEN);
  if(!preload){
    strcpy(preload_env, LIB_FILE);
  }else{
    strcat(preload_env, preload);
  }
  if(old_preload){
    n = strlen(preload_env);
    preload_env[n] = ':';
    preload_env[n+1] = '\0';
    strcat(preload_env, old_preload);
  }
  setenv("LD_PRELOAD", preload_env, 1);
  execvp(argv[optind], argv + optind);

  return 0;
}
