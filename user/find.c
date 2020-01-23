//
// Created by AriesDevil on 2020/1/18.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

char *path_join(char *s, const char *t) {
  int slen = strlen(s);
  int tlen = strlen(t);

  char *p = (char *)malloc(slen+ 1 + tlen + 1);
  strcpy(p, s);
  *(p+slen) = '/';
  strcpy(p+slen+1, t);
  *(p+slen+1+tlen) = '\0';
  return p;
}

char *basename(char *path) {
  int len = strlen(path);
  int i;
  for (i = len -1 ; path[i] != '/' && i >= 0; i--)
    ;
  return path + i + 1;
}

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void find(char *path, char *name) {

  int fd;
  struct dirent de;
  struct stat st;
//  char *ban[] = {".", ".."};
  if((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
  case T_FILE:
    if (strcmp(basename(path), name) == 0) {
      printf("%s\n", path);
    }
    break;
  case T_DIR:
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if(de.inum == 0)
        continue;
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      char *p = path_join(path, de.name);
      find(p, name);
      free(p);
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {

  if(argc < 3) {
    fprintf(2, "Usage: find path file");
    exit();
  }
  find(argv[1], argv[2]);
  exit();
}
