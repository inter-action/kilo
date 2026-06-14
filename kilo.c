#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define IS_LLDB (false)
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
  int screenrows;
  int screencols;

  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
  // We could use atexit() to clear the screen when our program exits,
  // but then the error message printed by die() would get erased right after
  // printing it.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  };
}

void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
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

char editorReadKey() {
  int nread;
  char c;
  while (true) {
    nread = read(STDIN_FILENO, &c, 1);
    if (nread != 1) {
      break;
    }
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (IS_LLDB) {
    *rows = 80;
    *cols = 80;
    return 0;
  } else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // this always return 0 inside lldb
    // return getCursorPosition(rows, cols);
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~", 1);

    if (y < E.screenrows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  // clear screen
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H]", 3);
}

/*** input ***/
void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    exit(0);
    break;
  }
}

/*** init ***/

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
    die("getWindowSize");
  }
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  while (true) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
