//
// Created by AriesDevil on 2020/1/18.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
  int parent_fd[2];
  int child_fd[2];
  if (pipe(parent_fd) < 0 || pipe(child_fd) < 0) {
    fprintf(2, "pipe error\n");
    exit();
  }
  char msg[1];
  if (fork()) {
    write(parent_fd[1], msg, sizeof(msg));
    if (read(child_fd[0], msg, 1) > 0) {
      fprintf(1, "%d: received pong\n", getpid());
    }
  } else {
    read(parent_fd[0], msg, 1);
    fprintf(1, "%d: received ping\n", getpid());
    write(child_fd[1], msg, 1);
  }
  exit();
}