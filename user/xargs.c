//
// Created by AriesDevil on 2020/1/21.
//
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

void xargs_exec(char *path, char **args) {
    if (fork()) {
        wait();
    } else {
        exec(path, args);
    }
}

int main(int argc, char *argv[]) {
    char **args = malloc(MAXARG * sizeof(char *));
    int count = 0;

    char *stdin_buf = malloc(128 * sizeof(char));
    int i = 0;
    while (1) {
        char tmp[1];
        int n = read(0, tmp, 1);
        if (n < 0) {
            fprintf(2, "read from stdin failed");
            exit();
        }
        if (n == 0) {
            break;
        }
        if (tmp[0] == '\n' || tmp[0] == ' ' || tmp[0] == '\t') {
            i = 0;
            args[count] = stdin_buf;
            stdin_buf = malloc(128 * sizeof(char));
            count++;
            continue;
        }
        stdin_buf[i] = tmp[0];
        i++;
    }

    char **xargs = malloc((argc-1 + count) * sizeof(char *));
    for (int k = 1; k < argc; ++k) {
        xargs[k - 1] = argv[k];
    }
    for (int l = 0; l < count; ++l) {
        xargs[argc - 1 + l] = args[l];
    }

    xargs_exec(argv[1], xargs);

    exit();
}