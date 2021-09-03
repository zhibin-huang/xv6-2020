#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  char c;
  char buf[128];
  char *c_argv[MAXARG];

  if (argc < 2) {
    fprintf(2, "usage: xargs commend\n");
    exit(0);
  }

  int i, j;
  for(i = 0; i < argc - 1; ++i){
    c_argv[i] = argv[i + 1];
  }
  c_argv[i] = buf; 
  
  j = 0;
  while (read(0, &c, 1)) {
    if (c == '\n') {
      buf[j] = 0;
      c_argv[i + 1] = 0;
      if (fork() == 0) {
        exec(argv[1], c_argv);
      } else {
        i = argc - 1, j = 0;
        wait(0);
      }
    } else {
      if (c == ' ') {
        buf[j] = 0;
      } else {
        if (buf[j] == 0)
          c_argv[++i] = &buf[++j];
        buf[j++] = c;
      }
    }
  }

  exit(0);
}