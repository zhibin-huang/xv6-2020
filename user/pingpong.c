#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int p2c[2], c2p[2];
    char msg[20];
    pipe(p2c);
    pipe(c2p);

    if(fork() == 0){
        close(p2c[1]);
        close(c2p[0]);
        read(p2c[0], msg ,14);
        printf("%d: %s\n", getpid(), msg);
        write(c2p[1], "received pong", 14);
        exit(0);
    }
    close(p2c[0]);
    close(c2p[1]);
    write(p2c[1], "received ping", 14);
    read(c2p[0], msg, 14);
    printf("%d: %s\n", getpid(), msg);
    exit(0);
}