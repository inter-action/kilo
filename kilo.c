#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)


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

char editorReadKey()
{
  int nread;
  char c;
  while(true){
    nread = read(STDIN_FILENO, &c, 1);
    if (nread != 1) {break;}
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return c;
}

/*** input ***/
void editorProcessKeypress()
{
  char c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      exit(0);
      break;
  }

}



/*** init ***/

int main(int argc, char *argv[]) {
  enableRawMode();

  while (true) {
    editorProcessKeypress();
  }
  return 0;
}
