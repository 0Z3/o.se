/*
  Copyright (c) 2019-22 John MacCallum Permission is hereby granted,
  free of charge, to any person obtaining a copy of this software
  and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute,
  sublicense, and/or sell copies of the Software, and to permit
  persons to whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* For atexit() function to check if restore is needed*/
static int rawmode = 0; 
/* In order to restore at exit.*/
static struct termios orig_termios; 

static void ose_termCooked(void)
{
    /* Don't even check the return value as it's too late. */
    if(rawmode
       && tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios) != -1)
    {
        rawmode = 0;
    }
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void ose_termAtExit(void)
{
    ose_termCooked();
}

int ose_termRaw(void)
{
    struct termios raw;

    if(rawmode)
    {
        return 1;
    }
    if(!isatty(STDIN_FILENO))
    {
        goto fatal;
    }
    if(tcgetattr(STDIN_FILENO,&orig_termios) == -1)
    {
        goto fatal;
    }
    raw = orig_termios;
    /* input modes: no break, no CR to NL, no parity check, no strip
       char, no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    /* raw.c_oflag &= ~(OPOST); */
    raw.c_oflag &= (OPOST | ONLCR);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended
       functions, no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* raw.c_lflag |= ISIG; */
    /* raw.c_cc[VINTR] = 3; */ /* Ctrl-C interrupts process */
    /* control chars - set return condition: min number of bytes and
       timer.  We want read to return every single byte, without
       timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) < 0)
    {
        goto fatal;
    }
    rawmode = 1;
    atexit(ose_termAtExit);
    return 0;

fatal:
    return -1;
}

/* int ose_termRead(void) */
/* { */
/*     unsigned char c; */
/*     fflush(stdout); */
/*     /\* ose_termRaw(); *\/ */
/*     if(read(STDIN_FILENO, &c, 1) == 1) */
/*     { */
/*         return (unsigned int)c; */
/*     } */
/*     return 0; */
/* } */

int32_t ose_termRead(int fd, int32_t buflen, char *buf)
{
    return read(fd, buf, buflen);
}
