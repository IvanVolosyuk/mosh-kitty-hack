#include <fcntl.h>
#include <stdio.h>  // includes
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "common.h"

void put(std::string esc) {
  char* tmux = getenv("TMUX");

  if (tmux != nullptr) {
    fprintf(stderr, "TMUX is set\n");
    esc = "\ePtmux;\e" + esc + "\e\\";
  }
  ssize_t res = write(1, esc.data(), esc.size());
  fprintf(stderr, "Written %ld bytes of %ld\n", res, esc.size());
  fflush(stdout);
}

int main(int argc, char** argv) {
  dup2(2, 100);
  // Log to /dev/null by default
  int er = open("/dev/null", O_CREAT | O_WRONLY | O_APPEND, 0777);
  // dup2(er, 2);
  for (int i = 0; i < argc; i++) {
    fprintf(stderr, "%s\n", argv[i]);
  }

  int fd = open_tty();
  fprintf(stderr, "tty %d\n", fd);
  if (fd != -1) {
    dup2(fd, 1);
  } else {
    // FIXME: Probably separate script should be used instead?
    // From TMUX <c-a> y
    // bind-key -T copy-mode-vi y send-keys -X copy-pipe-and-cancel 'yank
    // >#{pane_tty}'
    fprintf(stderr, "Writing to stdout\n");
  }

  std::string s(std::istreambuf_iterator<char>(std::cin), {});
  // put("\e]52;c;!\a");
  int SZ = 128001;
  for (size_t i = 0; i < s.length(); i += SZ) {
    const char* start = s.data() + i;
    size_t end = std::min(i + SZ, s.length());
    size_t len = end - i;
    put("\e]52;c;" + base64_encode((unsigned char*)start, len) + "\a");
  }
  // usleep(100000);

  exit(0);
}
