#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "serial.h"

int put_char(int fd, unsigned char c)
{
  return write(fd, &c, 1);
}

unsigned int get_file_size(const char *path)
{
  unsigned long filesize = -1;
  struct stat statbuff;
  if(stat(path, &statbuff) < 0)
    return filesize;
  else
    filesize = statbuff.st_size;
  return filesize;
}

