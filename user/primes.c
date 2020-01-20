//
// Created by AriesDevil on 2020/1/18.
//
#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  int pipe_fd[2];
  if (pipe(pipe_fd) < 0) {
    fprintf(2, "pipe create error.\n");
    exit();
  }
  int parent[1];
  int child[1];
  if (fork()) {
    for (int i = 2; i <= 35; ++i) {
      parent[0] = i;
      write(pipe_fd[1], parent, sizeof(parent));
    }
  } else {
    while (1) {
      int reader = pipe_fd[0];
      if (read(reader, parent, sizeof(parent)) == 0) {
        exit();
      }
      int p = parent[0];
      if (p >= 35) {
        fprintf(1, "reach to 35, exit.\n");
        exit();
      }
      // 关闭pipe_fd的写端，在子进程关闭不影响父进程
      close(pipe_fd[1]);

      if (pipe(pipe_fd) < 0) {
        fprintf(2, "pipe create err in pid: %d\n", getpid());
        exit();
      }

      if (fork()) {
        fprintf(1, "%d: prime %d\n", getpid(), p);
        while (1) {
          if (read(reader, child, sizeof(child)) == 0) {
            exit();
          }
          int n = child[0];
          if (n % p) {
            write(pipe_fd[1], child, sizeof(child));
          }
        }
      }
    }
  }
  exit();
}