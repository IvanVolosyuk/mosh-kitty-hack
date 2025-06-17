#include <fcntl.h>
#include <stdio.h>  // includes
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "common.h"

int match_prefix(const std::string& s) {
  if (s.size() < 7) return false;
  if (s[0] != '\e' || s[1] != ']' || s[2] != '5' || s[3] != '2' || s[4] != ';')
    return false;
  if (s[5] == ';') {
    // shoft_version
    return 6;
  }
  if (s[6] == ';' && (s[5] >= 'a' && s[5] <= 'z')) {
    return 7;
  }
  return 0;
}

int main(int argc, char** argv) {
  // Log to /dev/null by default
  int er = open("/dev/null", O_CREAT | O_WRONLY | O_APPEND, 0777);

  std::string reg = "c";
  bool clear = false;
  bool debug = false;

  if (argc == 2) {
    std::string args = argv[1];
    for (char c : args) {
      switch (c) {
        case '-':
          reg = "";
          break;
        case 'z':
        case '!':
          clear = true;
          break;
        case 'c':
          reg = "c";
          break;
        case 'p':
          reg = "p";
          break;
        case 'd':
          debug = true;
          break;
      }
    }
  }

  if (!debug) dup2(er, 2);
  for (int i = 0; i < argc; i++) {
    fprintf(stderr, "%s\n", argv[i]);
  }

  int fd = open_tty();

  // Using logic from https://github.com/tmux/tmux/issues/1477
  struct termios t_orig;
  struct termios t;
  int res = tcgetattr(fd, &t);
  if (res == -1) {
    perror("get term attr");
  }
  memcpy(&t_orig, &t, sizeof(termios));
  t.c_lflag &= ~ECHO;
  t.c_lflag &= ~ICANON;
  res = tcsetattr(fd, TCSANOW, &t);
  if (res == -1) {
    perror("set term attr");
  }

  if (getenv("TMUX")) {
    // Hack for mosh and patched kitty.
    std::string reset = "\033Ptmux;\033\033]52;c;!\a\033\\";
    write(fd, reset.data(), reset.size());
    usleep(5000);

    res = tcsetattr(fd, TCSANOW, &t_orig);
    if (res == -1) {
      perror("restore term attr");
    }
    // usleep(100000);

    fprintf(stderr, "Is TMUX\n");
    // Borrow idea from https://github.com/rumpelsepp/oscclip
    system("tmux refresh-client -l");
    // Round-trip to kitty
    usleep(300000);
    fprintf(stderr, "Save buffer:\n");
    int res = system("tmux save-buffer -");
    exit(res);
  }

  std::string reset = "\e]52;c;!\a";
  write(fd, reset.data(), reset.size());

  std::string req = "\e]52;" + reg + ";?\a";
  fprintf(stderr, "Write request: %s\n", req.substr(1).c_str());

  write(fd, req.data(), req.size());

  std::string encoded;

  fprintf(stderr, "Read response\n");

  bool found = false;
  while (!found) {
    const int buf_size = 4096;
    char buf[buf_size];
    size_t len = read(fd, buf, buf_size);
    fprintf(stderr, "len %ld\n", len);

    for (size_t i = 0; i < len; i++) {
      // Found end marker
      if (buf[i] == '\\') {
        encoded += std::string(buf, i);
        found = true;
        break;
      }
      encoded += std::string(buf, len);
    }
  }

  res = tcsetattr(fd, TCSANOW, &t_orig);
  if (res == -1) {
    perror("restore term attr");
  }

  int prefix = match_prefix(encoded);
  fprintf(stderr, "Matched prefix: %d\n", prefix);
  if (prefix == 0) {
    fprintf(stderr, "Header error[sz %ld]: ", encoded.size());
    for (size_t i = 0; i < std::min((size_t)20, encoded.size()); i++) {
      fprintf(stderr, "%d[%c] ", encoded[i],
              encoded[i] > 31 ? encoded[i] : '?');
    }
    exit(1);
  }

  if (encoded[encoded.size() - 1] != '\e') {
    fprintf(stderr, "Failure at the end of the selection\n");
    exit(1);
  }
  encoded = encoded.substr(prefix, encoded.size() - prefix - 1);
  std::string decoded = base64_decode(encoded);
  write(1, decoded.data(), decoded.size());
  exit(0);
}
