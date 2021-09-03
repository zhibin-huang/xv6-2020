#include "kernel/types.h"
#include "user/user.h"

void prime(int pfd[]) {
  int p, n;
  int next_pfd[2];
  if (read(pfd[0], &p, 4) != 0) {
    printf("prime %d\n", p);
  } else {
    close(pfd[0]);
    exit(0);
  }

  pipe(next_pfd);
  if (fork() == 0) {
    close(next_pfd[1]);
    prime(next_pfd);
  } else {
    close(next_pfd[0]);
    while (1) {
      if (read(pfd[0], &n, 4)) {
        if (n % p != 0)
          write(next_pfd[1], &n, 4);
      } else {
        close(next_pfd[1]);
        while (wait(0) != -1)
          ;
        exit(0);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int i, pfd[2];
  pipe(pfd);
  if (fork() == 0) {
    close(pfd[1]);
    prime(pfd);
  } else {
    close(pfd[0]);
    for (i = 2; i <= 35; ++i) {
      write(pfd[1], &i, 4);
    }
    close(pfd[1]);
  }

  while (wait(0) != -1)
    ;
  exit(0);
}