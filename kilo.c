#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  };
}

void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tcsetattr");
  }
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  // IXON, disable https://en.wikipedia.org/wiki/Software_flow_control
  // ICRNL, fix Ctrl-m = Enter
  // BRKINT, will cause a SIGINT signal to be sent to the program,
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  // OPOST, disable \n == \r\n
  raw.c_oflag &= ~(OPOST);
  //  It sets the character size (CS) to 8 bits per byte
  raw.c_cflag |= (CS8);

  // disable echo
  // The c_lflag field is for “local flags”
  // ICANON,
  // - There is an ICANON flag that allows us to turn off canonical mode.
  // - This means we will finally be reading input byte-by-byte, instead of
  // line-by-line.
  // ISIG, disable SIGINT, SIGTSTP IEXTEN,
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  // make read() unblock
  // The VMIN value sets the minimum number of bytes of input needed before
  // read() can return.
  raw.c_cc[VMIN] = 0;
  // The VTIME value sets the maximum amount of time to wait before read()
  // returns. It is in tenths of a second, 100 miliseconds
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

/*** init ***/

int main(int argc, char *argv[]) {
  enableRawMode();

  while (true) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
      die("read");
    }
    // iscntrl() tests whether a character is a control character.
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      // %d tells it to format the byte as a decimal number (its ASCII code),
      // and %c tells it to write out the byte directly, as a character.
      printf("%d ('%c') \r\n", c, c);
    }
    if (c == 'q')
      break;
  }
  return 0;
}
