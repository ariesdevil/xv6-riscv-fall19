#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
  if(argc <= 1) {
    fprintf(2, "usage: sleep duration\n");
    exit();
  }
  int duration = atoi(argv[1]);
  fprintf(2, "duration: %d\n", duration);
  sleep(duration);
  exit();
}