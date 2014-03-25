/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/msteveb/linenoise
 *   (forked from http://github.com/antirez/linenoise)
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2011, Steve Bennett <steveb at workware dot net dot au>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Bloat:
 * - Completion?
 *
 * Unix/termios
 * ------------
 * List of escape sequences used by this program, we do everything just
 * a few sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * CR (Carriage Return)
 *    Sequence: \r
 *    Effect: moves cursor to column 1
 *
 * The following are used to clear the screen: ESC [ H ESC [ 2 J
 * This is actually composed of two sequences:
 *
 * cursorhome
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED2 (Clear entire screen)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 * == For highlighting control characters, we also use the following two ==
 * SO (enter StandOut)
 *    Sequence: ESC [ 7 m
 *    Effect: Uses some standout mode such as reverse video
 *
 * SE (Standout End)
 *    Sequence: ESC [ 0 m
 *    Effect: Exit standout mode
 *
 * == Only used if TIOCGWINSZ fails ==
 * DSR/CPR (Report cursor position)
 *    Sequence: ESC [ 6 n
 *    Effect: reports current cursor position as ESC [ NNN ; MMM R
 *
 * win32/console
 * -------------
 * If __MINGW32__ is defined, the win32 console API is used.
 * This could probably be made to work for the msvc compiler too.
 * This support based in part on work by Jon Griffiths.
 */

#ifdef _WIN32 /* Windows platform, either MinGW or Visual Studio (MSVC) */
#include <windows.h>
#include <fcntl.h>
#define USE_WINCONSOLE
#ifdef __MINGW32__
#define HAVE_UNISTD_H
#else
/* Microsoft headers don't like old POSIX names */
#define strdup _strdup
#define snprintf _snprintf
#endif
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#define USE_TERMIOS
#define HAVE_UNISTD_H
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>

#include "linenoise.h"
#include "utf8.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096

#define ctrl(C) ((C) - '@')

/* Use -ve numbers here to co-exist with normal unicode chars */
enum {
    SPECIAL_NONE,
    SPECIAL_UP = -20,
    SPECIAL_DOWN = -21,
    SPECIAL_LEFT = -22,
    SPECIAL_RIGHT = -23,
    SPECIAL_DELETE = -24,
    SPECIAL_HOME = -25,
    SPECIAL_END = -26,
    SPECIAL_INSERT = -27,
    SPECIAL_PAGE_UP = -28,
    SPECIAL_PAGE_DOWN = -29
};

static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

/* Structure to contain the status of the current (being edited) line */
struct current {
    char *buf;  /* Current buffer. Always null terminated */
    int bufmax; /* Size of the buffer, including space for the null termination */
    size_t len;    /* Number of bytes in 'buf' */
    size_t chars;  /* Number of chars in 'buf' (utf-8 chars) */
    size_t pos;    /* Cursor position, measured in chars */
    int cols;   /* Size of the window, in chars */
    int rows;   /* Screen rows */
    const char *prompt;
    char *capture; /* Allocated capture buffer, or NULL for none. Always null terminated */
#if defined(USE_TERMIOS)
    int fd;     /* Terminal fd */
#elif defined(USE_WINCONSOLE)
    HANDLE outh; /* Console output handle */
    HANDLE inh; /* Console input handle */
    int x;      /* Current column during output */
    int y;      /* Current row */

    /*initial coordinates for displaying the buffer*/
    int ix;     
    int iy;     
#endif
};

static int fd_read(struct current *current);
static int getWindowSize(struct current *current);
static void set_current(struct current *current, const char *str);
static void refreshLine(const char *prompt, struct current *current);
static void refreshPage(const struct linenoiseCompletions * lc, struct current *current);
static void initLinenoiseLine(struct current *current);
static size_t new_line_numbers(size_t pos, int cols, size_t pchars);

void linenoiseHistoryFree(void) {
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
        history = NULL;
        history_len = 0;
    }
}

#if defined(USE_TERMIOS)
static void linenoiseAtExit(void);
static struct termios orig_termios; /* in order to restore at exit */
static int rawmode = 0; /* for atexit() function to check if restore is needed*/
static int atexit_registered = 0; /* register atexit just 1 time */

static const char *unsupported_term[] = {"dumb","cons25",NULL};

static int isUnsupportedTerm(void) {
    char *term = getenv("TERM");

    if (term) {
        int j;
        for (j = 0; unsupported_term[j]; j++) {
            if (strcmp(term, unsupported_term[j]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int enableRawMode(struct current *current) {
    struct termios raw;

    current->fd = STDIN_FILENO;
    current->cols = 0;

    if (!isatty(current->fd) || isUnsupportedTerm() ||
        tcgetattr(current->fd, &orig_termios) == -1) {
fatal:
        errno = ENOTTY;
        return -1;
    }

    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(current->fd,TCSADRAIN,&raw) < 0) {
        goto fatal;
    }
    rawmode = 1;
    return 0;
}

static void disableRawMode(struct current *current) {
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(current->fd,TCSADRAIN,&orig_termios) != -1)
        rawmode = 0;
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void) {
    if (rawmode) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &orig_termios);
    }
    linenoiseHistoryFree();
}

/* gcc/glibc insists that we care about the return code of write!
 * Clarification: This means that a void-cast like "(void) (EXPR)"
 * does not work.
 */
#define IGNORE_RC(EXPR) if (EXPR) {}

/* This is fdprintf() on some systems, but use a different
 * name to avoid conflicts
 */
static void fd_printf(int fd, const char *format, ...)
{
    va_list args;
    char buf[64];
    int n;

    va_start(args, format);
    n = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    IGNORE_RC(write(fd, buf, n));
}

static void clearScreen(struct current *current)
{
    fd_printf(current->fd, "\x1b[H\x1b[2J");
}

static void cursorToLeft(struct current *current)
{
    fd_printf(current->fd, "\r");
}

static int outputChars(struct current *current, const char *buf, int len)
{
    return write(current->fd, buf, len);
}

static int newLine(struct current *current)
{
    return outputChars(current, "\n", 1);
}

static void outputControlChar(struct current *current, char ch)
{
    fd_printf(current->fd, "\x1b[7m^%c\x1b[0m", ch);
}

static void eraseEol(struct current *current)
{
    fd_printf(current->fd, "\x1b[0K");
}

static void setCursorPos(struct current *current, int x)
{
    fd_printf(current->fd, "\r\x1b[%dC", x);
}

/**
 * Reads a char from 'fd', waiting at most 'timeout' milliseconds.
 *
 * A timeout of -1 means to wait forever.
 *
 * Returns -1 if no char is received within the time or an error occurs.
 */
static int fd_read_char(int fd, int timeout)
{
    struct pollfd p;
    unsigned char c;

    p.fd = fd;
    p.events = POLLIN;

    if (poll(&p, 1, timeout) == 0) {
        /* timeout */
        return -1;
    }
    if (read(fd, &c, 1) != 1) {
        return -1;
    }
    return c;
}

/**
 * Reads a complete utf-8 character
 * and returns the unicode value, or -1 on error.
 */
static int fd_read(struct current *current)
{
#ifdef USE_UTF8
    char buf[4];
    int n;
    int i;
    int c;

    if (read(current->fd, &buf[0], 1) != 1) {
        return -1;
    }
    n = utf8_charlen(buf[0]);
    if (n < 1 || n > 3) {
        return -1;
    }
    for (i = 1; i < n; i++) {
        if (read(current->fd, &buf[i], 1) != 1) {
            return -1;
        }
    }
    buf[n] = 0;
    /* decode and return the character */
    utf8_tounicode(buf, &c);
    return c;
#else
    return fd_read_char(current->fd, -1);
#endif
}

static int countColorControlChars(const char* prompt)
{
    /* ANSI color control sequences have the form:
     * "\x1b" "[" [0-9;]* "m"
     * We parse them with a simple state machine.
     */

    enum {
        search_esc,
        expect_bracket,
        expect_trail
    } state = search_esc;
    int len = 0, found = 0, flags_counter = 0;
    char ch;

    /* XXX: Strictly we should be checking utf8 chars rather than
     *      bytes in case of the extremely unlikely scenario where
     *      an ANSI sequence is part of a utf8 sequence.
     */
    while ((ch = *prompt++) != 0) {
        switch (state) {
        case search_esc:
            if (ch == '\x1b') {
                state = expect_bracket;
            } else {
                if(2>=(int)ch) {
                 flags_counter += 1;
                }
            }
            break;
        case expect_bracket:
            if (ch == '[') {
                state = expect_trail;
                /* 3 for "\e[ ... m" */
                len = 3;
                break;
            }
            state = search_esc;
            break;
        case expect_trail:
            if ((ch == ';') || ((ch >= '0' && ch <= '9'))) {
                /* 0-9, or semicolon */
                len++;
                break;
            }
            if (ch == 'm') {
                found += len;
            }
            state = search_esc;
            break;
        }
    }

    return found + flags_counter;
}

/**
 * Stores the current cursor column in '*cols'.
 * Returns 1 if OK, or 0 if failed to determine cursor pos.
 */
static int queryCursor(int fd, int* cols)
{
    /* control sequence - report cursor location */
    fd_printf(fd, "\x1b[6n");

    /* Parse the response: ESC [ rows ; cols R */
    if (fd_read_char(fd, 100) == 0x1b &&
        fd_read_char(fd, 100) == '[') {

        int n = 0;
        while (1) {
            int ch = fd_read_char(fd, 100);
            if (ch == ';') {
                /* Ignore rows */
                n = 0;
            }
            else if (ch == 'R') {
                /* Got cols */
                if (n != 0 && n < 1000) {
                    *cols = n;
                }
                break;
            }
            else if (ch >= 0 && ch <= '9') {
                n = n * 10 + ch - '0';
            }
            else {
                break;
            }
        }
        return 1;
    }

    return 0;
}

/**
 * Updates current->cols with the current window size (width)
 */
static int getWindowSize(struct current *current)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0) {
        current->cols = ws.ws_col;
        current->rows = ws.ws_row;
        return 0;
    }

    /* Failed to query the window size. Perhaps we are on a serial terminal.
     * Try to query the width by sending the cursor as far to the right
     * and reading back the cursor position.
     * Note that this is only done once per call to linenoise rather than
     * every time the line is refreshed for efficiency reasons.
     *
     * In more detail, we:
     * (a) request current cursor position,
     * (b) move cursor far right,
     * (c) request cursor position again,
     * (d) at last move back to the old position.
     * This gives us the width without messing with the externally
     * visible cursor position.
     */

    if (current->cols == 0) {
        int here;

        current->cols = 80;

        /* (a) */
        if (queryCursor (current->fd, &here)) {
            /* (b) */
            fd_printf(current->fd, "\x1b[999C");

            /* (c). Note: If (a) succeeded, then (c) should as well.
             * For paranoia we still check and have a fallback action
             * for (d) in case of failure..
             */
            if (!queryCursor (current->fd, &current->cols)) {
                /* (d') Unable to get accurate position data, reset
                 * the cursor to the far left. While this may not
                 * restore the exact original position it should not
                 * be too bad.
                 */
                fd_printf(current->fd, "\r");
            } else {
                /* (d) Reset the cursor back to the original location. */
                if (current->cols > here) {
                    fd_printf(current->fd, "\x1b[%dD", current->cols - here);
                }
            }
        } /* 1st query failed, doing nothing => default 80 */
    }

    return 0;
}

/**
 * If escape (27) was received, reads subsequent
 * chars to determine if this is a known special key.
 *
 * Returns SPECIAL_NONE if unrecognised, or -1 if EOF.
 *
 * If no additional char is received within a short time,
 * 27 is returned.
 */
static int check_special(int fd)
{
    int c = fd_read_char(fd, 50);
    int c2;

    if (c < 0) {
        return 27;
    }

    c2 = fd_read_char(fd, 50);
    if (c2 < 0) {
        return c2;
    }
    if (c == '[' || c == 'O') {
        /* Potential arrow key */
        switch (c2) {
            case 'A':
                return SPECIAL_UP;
            case 'B':
                return SPECIAL_DOWN;
            case 'C':
                return SPECIAL_RIGHT;
            case 'D':
                return SPECIAL_LEFT;
            case 'F':
                return SPECIAL_END;
            case 'H':
                return SPECIAL_HOME;
        }
    }
    if (c == '[' && c2 >= '1' && c2 <= '8') {
        /* extended escape */
        c = fd_read_char(fd, 50);
        if (c == '~') {
            switch (c2) {
                case '2':
                    return SPECIAL_INSERT;
                case '3':
                    return SPECIAL_DELETE;
                case '5':
                    return SPECIAL_PAGE_UP;
                case '6':
                    return SPECIAL_PAGE_DOWN;
                case '7':
                    return SPECIAL_HOME;
                case '8':
                    return SPECIAL_END;
            }
        }
        while (c != -1 && c != '~') {
            /* .e.g \e[12~ or '\e[11;2~   discard the complete sequence */
            c = fd_read_char(fd, 50);
        }
    }

    return SPECIAL_NONE;
}
static void initLinenoiseLine(struct current *current) {
// at moment is not used
}
#elif defined(USE_WINCONSOLE)
static DWORD orig_consolemode = 0;
static void initLinenoiseLine(struct current *current) {
  size_t pchars = utf8_strlen(current->prompt, strlen(current->prompt));

  CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
  // Get the screen buffer size.

  if (!GetConsoleScreenBufferInfo(current->outh, &csbiInfo))
  {
    printf("\nGetConsoleScreenBufferInfo failed %d\n", GetLastError());
    return ;
  }
  current->ix = pchars;
  current->iy = csbiInfo.dwCursorPosition.Y;
}

static int enableRawMode(struct current *current) {
    DWORD n;
    INPUT_RECORD irec;

    current->outh = GetStdHandle(STD_OUTPUT_HANDLE);
    current->inh = GetStdHandle(STD_INPUT_HANDLE);

    if (!PeekConsoleInput(current->inh, &irec, 1, &n)) {
        return -1;
    }
    if (getWindowSize(current) != 0) {
        return -1;
    }
    if (GetConsoleMode(current->inh, &orig_consolemode)) {
        SetConsoleMode(current->inh, ENABLE_PROCESSED_INPUT);
    }
    return 0;
}

static void disableRawMode(struct current *current)
{
    SetConsoleMode(current->inh, orig_consolemode);
}

static void clearScreen(struct current *current)
{
    COORD topleft = { 0, 0 };
    current->x = current->y = 0;
    DWORD n;

    FillConsoleOutputCharacter(current->outh, ' ',
        current->cols * current->rows, topleft, &n);
    FillConsoleOutputAttribute(current->outh,
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN,
        current->cols * current->rows, topleft, &n);
    SetConsoleCursorPosition(current->outh, topleft);
}

static void cursorToLeft(struct current *current)
{
    COORD pos = { 0, (SHORT)current->y };
    DWORD n;

    FillConsoleOutputAttribute(current->outh,
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN, current->cols, pos, &n);
    current->x = 0;
}
// only for debuging proposes
void writeCurrentValues(int a, int b, int c) {
    FILE * f = fopen("debug.txt", "a+");
    fprintf(f, "%d %d %d \n", a, b, c);
    fclose(f);
}
static int newLine(struct current *current)
{
      current->y += 1;
      CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
      SMALL_RECT srctScrollRect;
      // Get the screen buffer size.

      if (!GetConsoleScreenBufferInfo(current->outh, &csbiInfo))
      {
         printf("\nGetConsoleScreenBufferInfo failed %d\n", GetLastError());
         return 1;
      }
      //writeCurrentValues(current->y, csbiInfo.dwSize.Y, csbiInfo.dwCursorPosition.Y);
      if (csbiInfo.dwCursorPosition.Y + 1 == current->rows)  {
        srctScrollRect.Top = 0;
        srctScrollRect.Bottom = current->rows - 1;
        srctScrollRect.Left = 0;
        srctScrollRect.Right = current->cols ;
        int delta = 2;
        // The destination for the scroll rectangle is one row up.
        COORD coordDest;
        coordDest.X = 0;
        coordDest.Y = -delta;
     
        CONSOLE_SCREEN_BUFFER_INFO  consoleScreenBufferInfo;
        CHAR_INFO chiFill; 
        chiFill.Char.AsciiChar = (char)' ';
        chiFill.Char.UnicodeChar = (char)' ';
        if(GetConsoleScreenBufferInfo(current->outh, &consoleScreenBufferInfo)) {
          chiFill.Attributes = consoleScreenBufferInfo.wAttributes;
        } else {
          // Fill the bottom row with green blanks.
          chiFill.Attributes = BACKGROUND_GREEN | FOREGROUND_RED;
        }

        // Scroll up one line

        if (!ScrollConsoleScreenBuffer(
          current->outh,    // screen buffer handle
          &srctScrollRect,  // scrolling rectangle
          NULL,             // clipping rectangle
          coordDest,        // top left destination cell
          &chiFill))        // fill character and color
        {
          printf("ScrollConsoleScreenBuffer failed %d\n", GetLastError());
          return 1;
        }
        current->y =   csbiInfo.dwCursorPosition.Y - delta;
      }
    COORD pos = { (SHORT)0, (SHORT)current->y };
    DWORD n;
    FillConsoleOutputCharacter(current->outh, ' ', current->cols, pos, &n);
    SetConsoleCursorPosition(current->outh, pos);
    return 0;
}

static int outputChars(struct current *current, const char *buf, size_t len)
{
    COORD pos = { (SHORT)current->x, (SHORT)current->y };
    DWORD n;
#ifdef  UNICODE   
    LPWSTR wBuf = malloc(sizeof(LPWSTR)* (len + 1));
    memset(wBuf, '\0', sizeof(LPWSTR)* (len + 1));
#else
    LPCSTR wBuf = malloc(sizeof(LPCSTR)* (len + 1));
    memset(wBuf, '\0', sizeof(LPCSTR)* (len + 1));
#endif
    int wLen =  MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, len);
    BOOL isOk = WriteConsoleOutputCharacter(current->outh, wBuf, len, pos, &n);
    if (len > 0) {
      free(wBuf);
    }

   // current->x += len;  
    // current->x += len;
    return 0;
}

static void outputControlChar(struct current *current, char ch)
{
    COORD pos = { (SHORT)current->x, (SHORT)current->y };
    DWORD n;

    FillConsoleOutputAttribute(current->outh, BACKGROUND_INTENSITY, 2, pos, &n);
    outputChars(current, "^", 1);
    outputChars(current, &ch, 1);
}

static void eraseEol(struct current *current)
{
  COORD pos;
  DWORD n;
  int pchars = current->cols;
  size_t plen = utf8_strlen(current->buf, strlen(current->buf));
  size_t num_lines = new_line_numbers(current->chars, current->cols, plen);

  pos.X = plen;
  pos.Y = current->iy;

  FillConsoleOutputCharacter(current->outh, ' ', pchars, pos, &n);
  for (int j = 1; j <= num_lines; j++) {
    pos.X = 0;
    pos.Y = j + current->iy;
      FillConsoleOutputCharacter(current->outh, ' ', pchars, pos, &n);
    } 
  
}

static void setCursorPos(struct current *current, int x)
{
    COORD pos = { (SHORT)x, (SHORT)current->y };

    SetConsoleCursorPosition(current->outh, pos);
    current->x = x;
}

static int fd_read(struct current *current)
{
    while (1) {
        INPUT_RECORD irec;
        DWORD n;
        if (WaitForSingleObject(current->inh, INFINITE) != WAIT_OBJECT_0) {
            break;
        }
        if (!ReadConsoleInput (current->inh, &irec, 1, &n)) {
            break;
        }
        if (irec.EventType == KEY_EVENT && irec.Event.KeyEvent.bKeyDown) {
            KEY_EVENT_RECORD *k = &irec.Event.KeyEvent;
            if (k->dwControlKeyState & ENHANCED_KEY) {
                switch (k->wVirtualKeyCode) {
                 case VK_LEFT:
                    return SPECIAL_LEFT;
                 case VK_RIGHT:
                    return SPECIAL_RIGHT;
                 case VK_UP:
                    return SPECIAL_UP;
                 case VK_DOWN:
                    return SPECIAL_DOWN;
                 case VK_INSERT:
                    return SPECIAL_INSERT;
                 case VK_DELETE:
                    return SPECIAL_DELETE;
                 case VK_HOME:
                    return SPECIAL_HOME;
                 case VK_END:
                    return SPECIAL_END;
                 case VK_PRIOR:
                    return SPECIAL_PAGE_UP;
                 case VK_NEXT:
                    return SPECIAL_PAGE_DOWN;
                }
            }
            /* Note that control characters are already translated in AsciiChar */
            else {
#ifdef USE_UTF8
                return k->uChar.UnicodeChar;
#else
                return k->uChar.AsciiChar;
#endif
            }
        }
    }
    return -1;
}

static int countColorControlChars(const char* prompt)
{
    /* For windows we assume that there are no embedded ansi color
     * control sequences.
     */
    return 0;
}

static int getWindowSize(struct current *current)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(current->outh, &info)) {
        return -1;
    }
    current->cols = info.dwSize.X;
    current->rows = info.dwSize.Y;
    if (current->cols <= 0 || current->rows <= 0) {
        current->cols = 80;
        return -1;
    }
    current->y = info.dwCursorPosition.Y;
    current->x = info.dwCursorPosition.X;
    return 0;
}
#endif

static int utf8_getchars(char *buf, int c)
{
#ifdef USE_UTF8
    return utf8_fromunicode(buf, c);
#else
    *buf = c;
    return 1;
#endif
}

/**
 * Returns the unicode character at the given offset,
 * or -1 if none.
 */
static int get_char(struct current *current, size_t pos)
{
    if (pos >= 0 && pos < current->chars) {
        int c;
        int i = utf8_index(current->buf, pos);
        (void)utf8_tounicode(current->buf + i, &c);
        return c;
    }
    return -1;
}
static void displayItems(const struct linenoiseCompletions * lc, struct current *current, int max_len)
{ 
  getWindowSize(current);
  size_t wcols = current->cols; 
  int cols = max_len > wcols ? 1 : wcols/(max_len+2);
  int rows = (int)ceil((float)lc->len/cols);
  int i, j;
  for(i=0;i<rows; i++) {
    newLine(current);
    for(j=0; j<cols; j++){
      setCursorPos(current, j * (max_len + 2));
      size_t idx = j*rows +i;
      if(idx>=lc->len) {
         break;
      }
      const char * row_content = lc->cvec[j * rows + i];
      outputChars(current, row_content, strlen(row_content)); 
    }
  }
  newLine(current);
}

static void refreshPage(const struct linenoiseCompletions * lc, struct current *current)
{
    size_t j;
    size_t common_min_len = INT_MAX; 
    size_t max_len = 0;
    char * min_chars = NULL;
    for(j=0; j<lc->len; j++) {
      size_t j_len = utf8_strlen(lc->cvec[j],  (int)strlen(lc->cvec[j]));
      if(min_chars == NULL) {
        min_chars = lc->cvec[j];
        common_min_len = j_len;
        max_len = j_len;
      } else {
        /*
         * compute maximal length of common string 
         */ 
        size_t tmp_len = 0;
        char * c_min_char = min_chars;
        char * j_chars = lc->cvec[j];
        int k=0;
        while(c_min_char[k] == j_chars[k]) {
          tmp_len++;
          k += 1; 
        }
        if(common_min_len > tmp_len && tmp_len>0) {
                common_min_len  = tmp_len;
        }
        max_len = max_len < j_len ? j_len : max_len;
      } 
    }
    displayItems(lc, current, max_len);
    newLine(current);
    if(min_chars!=NULL) {
      // char * new_buf = strndup(min_chars, common_min_len);
      char * new_buf = malloc(common_min_len + 1);
      memcpy(new_buf, min_chars, common_min_len);
      new_buf[common_min_len] = '\0';
      set_current(current, new_buf); 
      // this is posible because set_current copies the given pointer
      free(new_buf);
    }
    initLinenoiseLine(current);
    refreshLine(current->prompt, current);
}
#ifndef USE_WINCONSOLE
static void refreshLine(const char *prompt, struct current *current)
{
    size_t plen;
    size_t pchars;
    int backup = 0;
    int i;
    const char *buf = current->buf;
    size_t chars = current->chars;
    size_t pos = current->pos;
    int b;
    int ch;
    int n;

    /* Should intercept SIGWINCH. For now, just get the size every time */
    getWindowSize(current);

    plen = strlen(prompt);
    pchars = utf8_strlen(prompt, plen);
    /* Scan the prompt for embedded ansi color control sequences and
     * discount them as characters/columns.
     */
    pchars -= countColorControlChars(prompt);

    /* Account for a line which is too long to fit in the window.
     * Note that control chars require an extra column
     */

    /* How many cols are required to the left of 'pos'?
     * The prompt, plus one extra for each control char
     */
    n = pchars + utf8_strlen(buf, current->len);
    b = 0;
    for (i = 0; i < pos; i++) {
        b += utf8_tounicode(buf + b, &ch);
        if (ch < ' ') {
            n++;
        }
    }

    /* If too many are needed, strip chars off the front of 'buf'
     * until it fits. Note that if the current char is a control character,
     * we need one extra col.
     */
    if (current->pos < current->chars && get_char(current, current->pos) < ' ') {
        n++;
    }

    while (n >= current->cols && pos > 0) {
        b = utf8_tounicode(buf, &ch);
        if (ch < ' ') {
            n--;
        }
        n--;
        buf += b;
        pos--;
        chars--;
    }

    /* Cursor to left edge, then the prompt */
    cursorToLeft(current);
    outputChars(current, prompt, plen);

    /* Now the current buffer content */

    /* Need special handling for control characters.
     * If we hit 'cols', stop.
     */
    b = 0; /* unwritted bytes */
    n = 0; /* How many control chars were written */
    for (i = 0; i < chars; i++) {
        int ch2;
        int w = utf8_tounicode(buf + b, &ch2);
        if (ch2 < ' ') {
            n++;
        }
        if (pchars + i + n >= current->cols) {
            break;
        }
        if (ch2 < ' ') {
            /* A control character, so write the buffer so far */
            outputChars(current, buf, b);
            buf += b + w;
            b = 0;
            outputControlChar(current, ch2 + '@');
            if (i < pos) {
                backup++;
            }
        }
        else {
            b += w;
        }
    }
    outputChars(current, buf, b);
    /* Erase to right, move cursor to original position */
    eraseEol(current);
    setCursorPos(current, pos + pchars + backup );
}
#endif

static size_t new_line_numbers(size_t pos, int cols, size_t pchars) 
{
  if (pos < cols - pchars)
  {
    return 0;
  }
  else {
    return ((pos - (cols - pchars)) / cols) + 1;
  }
}

int next_allowed_x(size_t pos, int cols, int pchars)
{
  if (pos < cols - pchars)
  {
    return pos + pchars;
  }
  else {
    return (pos - (cols - pchars)) % (cols);
  }
}
#ifdef USE_WINCONSOLE
static void setCursorPosXY(struct current *current, int x, int y)
{
  COORD pos = { (SHORT)x, (SHORT)y };

  SetConsoleCursorPosition(current->outh, pos);
  current->x = x;
  current->y = y;
}
static int scrollWhenNeed(struct current * current, int lines, int forceScroll)
{
  CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
  SMALL_RECT srctScrollRect;
  // Get the screen buffer size.

  if (!GetConsoleScreenBufferInfo(current->outh, &csbiInfo))
  {
    printf("\nGetConsoleScreenBufferInfo failed %d\n", GetLastError());
    return 1;
  }

  if (forceScroll || (csbiInfo.dwCursorPosition.Y + 1 == current->rows))  {
    srctScrollRect.Top = 0;
    srctScrollRect.Bottom = current->rows - 1;
    srctScrollRect.Left = 0;
    srctScrollRect.Right = current->cols;
    // The destination for the scroll rectangle is one row up.
    COORD coordDest;
    coordDest.X = 0;
    coordDest.Y = -lines;

    CONSOLE_SCREEN_BUFFER_INFO  consoleScreenBufferInfo;
    CHAR_INFO chiFill;
    chiFill.Char.AsciiChar = (char)' ';
    chiFill.Char.UnicodeChar = (char)' ';
    if (GetConsoleScreenBufferInfo(current->outh, &consoleScreenBufferInfo)) {
      chiFill.Attributes = consoleScreenBufferInfo.wAttributes;
    }
    else {
      // Fill the bottom row with green blanks.
      chiFill.Attributes = BACKGROUND_GREEN | FOREGROUND_RED;
    }

    // Scroll up one line

    if (!ScrollConsoleScreenBuffer(
      current->outh,    // screen buffer handle
      &srctScrollRect,  // scrolling rectangle
      NULL,             // clipping rectangle
      coordDest,        // top left destination cell
      &chiFill))        // fill character and color
    {
      printf("ScrollConsoleScreenBuffer failed %d\n", GetLastError());
      return 1;
    }
    current->y = current->iy -= lines;
    return 1;
  }
  return 0;
}

static void refreshLine(const char *prompt, struct current *current)
{
  size_t plen;
  size_t pchars;
  const char *buf = current->buf;
  size_t chars = current->chars;
  size_t pos = current->pos;
  
  getWindowSize(current);
  plen = (int)strlen(prompt);
  pchars = utf8_strlen(prompt, plen);
  
  /* before something is displayed the old contet is Erased  */
  eraseEol(current);

  setCursorPosXY(current, 0, current->iy);
  outputChars(current, prompt, pchars);

  /*move cursor to initial coordinates of the editing line*/
  setCursorPosXY(current, current->ix, current->iy);
  /*when cursor is at the last line of the buffer scroll the content n lines above*/
  scrollWhenNeed(current, new_line_numbers(current->chars, current->cols, pchars), 0);
  /*show characters*/
  outputChars(current, buf, chars);
  int new_x = next_allowed_x(pos, current->cols, pchars);
  size_t new_lines = new_line_numbers(pos, current->cols, pchars);
  size_t new_y = current->y + new_lines;

  if (new_y >= current->rows) 
  {
    // scrollWhenNeed(current, 1 + (new_lines - 20), 1); 
    scrollWhenNeed(current, 1, 1); 
    new_y = current->rows - 1;
  }

  setCursorPosXY(current, new_x, new_y);
}
#endif

static void set_current(struct current *current, const char *str)
{
  memset(current->buf, '\0', current->bufmax);
    strncpy(current->buf, str, current->bufmax);
    current->buf[current->bufmax - 1] = 0;
    current->len = strlen(current->buf);
    current->pos = current->chars = utf8_strlen(current->buf, current->len);
}

static int has_room(struct current *current, int bytes)
{
    return current->len + bytes < current->bufmax - 1;
}

/**
 * Removes the char at 'pos'.
 *
 * Returns 1 if the line needs to be refreshed, 2 if not
 * and 0 if nothing was removed
 */
static int remove_char(struct current *current, size_t pos)
{
    if (pos >= 0 && pos < current->chars) {
        int p1, p2;
        int ret = 1;
        p1 = utf8_index(current->buf, pos);
        p2 = p1 + utf8_index(current->buf + p1, 1);

#ifdef USE_TERMIOS
        /* optimise remove char in the case of removing the last char */
        if (current->pos == pos + 1 && current->pos == current->chars) {
            if (current->buf[pos] >= ' ' && utf8_strlen(current->prompt, strlen(current->prompt)) + utf8_strlen(current->buf, current->len) < current->cols - 1) {
                ret = 2;
                fd_printf(current->fd, "\b \b");
            }
        }
#endif

        /* Move the null char too */
        memmove(current->buf + p1, current->buf + p2, current->len - p2 + 1);
        current->len -= (p2 - p1);
        current->chars--;

        if (current->pos > pos) {
            current->pos--;
        }
        return ret;
    }
    return 0;
}

/**
 * Insert 'ch' at position 'pos'
 *
 * Returns 1 if the line needs to be refreshed, 2 if not
 * and 0 if nothing was inserted (no room)
 */
static int insert_char(struct current *current, size_t pos, int ch)
{
  char buf[3] = {'\0','\0','\0'};
    int n = utf8_getchars(buf, ch);

    if (has_room(current, n) && pos >= 0 && pos <= current->chars) {
        int p1, p2;
        int ret = 1;
        p1 = utf8_index(current->buf, pos);
        p2 = p1 + n;

#ifdef USE_TERMIOS
        /* optimise the case where adding a single char to the end and no scrolling is needed */
        if (current->pos == pos && current->chars == pos) {
            if (ch >= ' ' && utf8_strlen(current->prompt, strlen(current->prompt)) + utf8_strlen(current->buf, current->len) < current->cols - 1) {
                IGNORE_RC(write(current->fd, buf, n));
                ret = 2;
            }
        }
#endif

        memmove(current->buf + p2, current->buf + p1, current->len - p1);
        memcpy(current->buf + p1, buf, n);
        current->len += n;

        current->chars++;
        if (current->pos >= pos) {
            current->pos++;
        }
        return ret;
    }
    return 0;
}

/**
 * Captures up to 'n' characters starting at 'pos' for the cut buffer.
 *
 * This replaces any existing characters in the cut buffer.
 */
static void capture_chars(struct current *current, size_t pos, size_t n)
{
    if (pos >= 0 && (pos + n - 1) < current->chars) {
        int p1 = utf8_index(current->buf, pos);
        int nbytes = utf8_index(current->buf + p1, n);

        if (nbytes) {
            free(current->capture);
            /* Include space for the null terminator */
            current->capture = (char *)malloc(nbytes + 1);
            memcpy(current->capture, current->buf + p1, nbytes);
            current->capture[nbytes] = '\0';
        }
    }
}

/**
 * Removes up to 'n' characters at cursor position 'pos'.
 *
 * Returns 0 if no chars were removed or non-zero otherwise.
 */
static size_t remove_chars(struct current *current, size_t pos, size_t n)
{
    size_t removed = 0;

    /* First save any chars which will be removed */
    capture_chars(current, pos, n);

    while (n-- && remove_char(current, pos)) {
        removed++;
    }
    return removed;
}
/**
 * Inserts the characters (string) 'chars' at the cursor position 'pos'.
 *
 * Returns 0 if no chars were inserted or non-zero otherwise.
 */
static size_t insert_chars(struct current *current, size_t pos, const char *chars)
{
    size_t inserted = 0;

    while (*chars) {
        int ch;
        int n = utf8_tounicode(chars, &ch);
        if (insert_char(current, pos, ch) == 0) {
            break;
        }
        inserted++;
        pos++;
        chars += n;
    }
    return inserted;
}

#ifndef NO_COMPLETION
static linenoiseCompletionCallback *completionCallback = NULL;

static void beep() {
#ifdef USE_TERMIOS
    fprintf(stderr, "\x7");
    fflush(stderr);
#endif
}

static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    free(lc->cvec);
}

static int completeLine(struct current *current) {
    linenoiseCompletions lc = { 0, NULL, 0 };
    int c = 0;

    completionCallback(current->buf,&lc);
    if (lc.len == 0) {
        beep();
    } else {
        if(lc.len>1 && lc.multiLine) {
           refreshPage(&lc, current);
           freeCompletions(&lc);
            return c;
        }
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                struct current tmp = *current;
                tmp.buf = lc.cvec[i];
                tmp.pos = tmp.len = strlen(tmp.buf);
                tmp.chars = utf8_strlen(tmp.buf, tmp.len);
                refreshLine(current->prompt, &tmp);
            } else { 
                refreshLine(current->prompt, current);
            }

            c = fd_read(current);
            if (c == -1) {
                break;
            }

            switch(c) {
                case '\t': /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) beep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.len) {
                        refreshLine(current->prompt, current);
                    }
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.len) {
                        set_current(current,lc.cvec[i]);
                    }
                    stop = 1;
                    break;
            }
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
    completionCallback = fn;
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    lc->cvec = (char **)realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    lc->cvec[lc->len++] = strdup(str);
}

#endif


#ifdef USE_WINCONSOLE
static void moveCursor(struct current * current, int x, int y) {
    COORD pos = { (SHORT)x, (SHORT)y };
    if (SetConsoleCursorPosition(current->outh, pos)) {
      current->x = x;
      current->y = y;
    }
}
static void moveCursorToLeft(struct current * current) {
  refreshLine(current->prompt, current);
}

static void moveCursorToRight(struct current * current) {
    refreshLine(current->prompt, current);  
}
#else
static void moveCursorToLeft(struct current * current) {
    refreshLine(current->prompt, current);
}
static void moveCursorToRight(struct current * current) {
    refreshLine(current->prompt, current);
}
#endif

static size_t linenoiseEdit(struct current *current) {
    int history_index = 0;

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");

    set_current(current, "");
    refreshLine(current->prompt, current);

    while(1) {
        int dir = -1;
        int c = fd_read(current);

#ifndef NO_COMPLETION
        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == '\t' && current->pos == current->chars && completionCallback != NULL) {
            c = completeLine(current);
            initLinenoiseLine(current);
            /* Return on errors */
            if (c < 0) return current->len;
            /* Read next character when 0 */
            if (c == 0) continue;
        }
#endif

process_char:
        if (c == -1) return current->len;
#ifdef USE_TERMIOS
        if (c == 27) {   /* escape sequence */
            c = check_special(current->fd);
        }
#endif
        switch(c) {
        case '\r':    /* enter */
            history_len--;
            free(history[history_len]);
            return current->len;
        case ctrl('C'):     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case 127:   /* backspace */
        case ctrl('H'):
          eraseEol(current);
            if (remove_char(current, current->pos - 1) == 1) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('D'):     /* ctrl-d */
            if (current->len == 0) {
                /* Empty line, so EOF */
                history_len--;
                free(history[history_len]);
                return -1;
            }
            /* Otherwise fall through to delete char to right of cursor */
        case SPECIAL_DELETE:
            if (remove_char(current, current->pos) == 1) {
                refreshLine(current->prompt, current);
            }
            break;
        case SPECIAL_INSERT:
            /* Ignore. Expansion Hook.
             * Future possibility: Toggle Insert/Overwrite Modes
             */
            break;
        case ctrl('W'):    /* ctrl-w, delete word at left. save deleted chars */
            /* eat any spaces on the left */
        {   
                size_t pos = current->pos;
                current->pos = current->chars;
                eraseEol(current);
                current->pos = pos;
                while (pos > 0 && get_char(current, pos - 1) == ' ') {
                    pos--;
                }

                /* now eat any non-spaces on the left */
                while (pos > 0 && get_char(current, pos - 1) != ' ') {
                    pos--;
                }

                if (remove_chars(current, pos, current->pos - pos)) {
                    refreshLine(current->prompt, current);
                }
            }
            break;
        case ctrl('R'):    /* ctrl-r */
            {
                /* Display the reverse-i-search prompt and process chars */
                char rbuf[50];
                char rprompt[80];
                int rchars = 0;
                size_t rlen = 0;
                int searchpos = history_len - 1;

                rbuf[0] = 0;
                while (1) {
                    int n = 0;
                    const char *p = NULL;
                    int skipsame = 0;
                    int searchdir = -1;

                    snprintf(rprompt, sizeof(rprompt), "(reverse-i-search)'%s': ", rbuf);
                    refreshLine(rprompt, current);
                    c = fd_read(current);
                    if (c == ctrl('H') || c == 127) {
                        if (rchars) {
                            int p2 = utf8_index(rbuf, --rchars);
                            rbuf[p2] = 0;
                            rlen = strlen(rbuf);
                        }
                        continue;
                    }
#ifdef USE_TERMIOS
                    if (c == 27) {
                        c = check_special(current->fd);
                    }
#endif
                    if (c == ctrl('P') || c == SPECIAL_UP) {
                        /* Search for the previous (earlier) match */
                        if (searchpos > 0) {
                            searchpos--;
                        }
                        skipsame = 1;
                    }
                    else if (c == ctrl('N') || c == SPECIAL_DOWN) {
                        /* Search for the next (later) match */
                        if (searchpos < history_len) {
                            searchpos++;
                        }
                        searchdir = 1;
                        skipsame = 1;
                    }
                    else if (c >= ' ') {
                        if (rlen >= (int)sizeof(rbuf) + 3) {
                            continue;
                        }

                        n = utf8_getchars(rbuf + rlen, c);
                        rlen += n;
                        rchars++;
                        rbuf[rlen] = 0;

                        /* Adding a new char resets the search location */
                        searchpos = history_len - 1;
                    }
                    else {
                        /* Exit from incremental search mode */
                        break;
                    }

                    /* Now search through the history for a match */
                    for (; searchpos >= 0 && searchpos < history_len; searchpos += searchdir) {
                        p = strstr(history[searchpos], rbuf);
                        if (p) {
                            /* Found a match */
                            if (skipsame && strcmp(history[searchpos], current->buf) == 0) {
                                /* But it is identical, so skip it */
                                continue;
                            }
                            /* Copy the matching line and set the cursor position */
                            set_current(current,history[searchpos]);
                            current->pos = utf8_strlen(history[searchpos], p - history[searchpos]);
                            break;
                        }
                    }
                    if (!p && n) {
                        /* No match, so don't add it */
                        rchars--;
                        rlen -= n;
                        rbuf[rlen] = 0;
                    }
                }
                if (c == ctrl('G') || c == ctrl('C')) {
                    /* ctrl-g terminates the search with no effect */
                    set_current(current, "");
                    c = 0;
                }
                else if (c == ctrl('J')) {
                    /* ctrl-j terminates the search leaving the buffer in place */
                    c = 0;
                }
                /* Go process the char normally */
                refreshLine(current->prompt, current);
                goto process_char;
            }
            break;
        case ctrl('T'):    /* ctrl-t */
            if (current->pos > 0 && current->pos <= current->chars) {
                /* If cursor is at end, transpose the previous two chars */
                int fixer = (current->pos == current->chars);
                c = get_char(current, current->pos - fixer);
                remove_char(current, current->pos - fixer);
                insert_char(current, current->pos - 1, c);
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('V'):    /* ctrl-v */
            if (has_room(current, 3)) {
                /* Insert the ^V first */
                if (insert_char(current, current->pos, c)) {
                    refreshLine(current->prompt, current);
                    /* Now wait for the next char. Can insert anything except \0 */
                    c = fd_read(current);

                    /* Remove the ^V first */
                    remove_char(current, current->pos - 1);
                    if (c != -1) {
                        /* Insert the actual char */
                        insert_char(current, current->pos, c);
                    }
                    refreshLine(current->prompt, current);
                }
            }
            break;
        case ctrl('B'):
        case SPECIAL_LEFT:
            if (current->pos > 0) {
                current->pos--;
                moveCursorToLeft(current);
                // refreshLine(current->prompt, current);
            }
            break;
        case ctrl('F'):
        case SPECIAL_RIGHT:
            if (current->pos < current->chars) {
                current->pos++;
                moveCursorToRight(current);
                //refreshLine(current->prompt, current);
            }
            break;
        case SPECIAL_PAGE_UP:
          dir = history_len - history_index - 1; /* move to start of history */
          goto history_navigation;
        case SPECIAL_PAGE_DOWN:
          dir = -history_index; /* move to 0 == end of history, i.e. current */
          goto history_navigation;
        case ctrl('P'):
        case SPECIAL_UP:
            dir = 1;
          goto history_navigation;
        case ctrl('N'):
        case SPECIAL_DOWN:
history_navigation:
            if (history_len > 1) {
                /* Update the current history entry before to
                 * overwrite it with tne next one. */
                free(history[history_len - 1 - history_index]);
                history[history_len - 1 - history_index] = strdup(current->buf);
                /* Show the new entry */
                history_index += dir;
                if (history_index < 0) {
                    history_index = 0;
                    break;
                } else if (history_index >= history_len) {
                    history_index = history_len - 1;
                    break;
                }
                eraseEol(current);
                set_current(current, history[history_len - 1 - history_index]);
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('A'): /* Ctrl+a, go to the start of the line */
        case SPECIAL_HOME:
            current->pos = 0;
            refreshLine(current->prompt, current);
            break;
        case ctrl('E'): /* ctrl+e, go to the end of the line */
        case SPECIAL_END:
            current->pos = current->chars;
            refreshLine(current->prompt, current);
            break;
        case ctrl('U'): /* Ctrl+u, delete to beginning of line, save deleted chars. */
          eraseEol(current);
          if (remove_chars(current, 0, current->pos)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('K'): /* Ctrl+k, delete from current to end of line, save deleted chars. */
          eraseEol(current);
          if (remove_chars(current, current->pos, current->chars - current->pos)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('Y'): /* Ctrl+y, insert saved chars at current position */
            if (current->capture && insert_chars(current, current->pos, current->capture)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('L'): /* Ctrl+L, clear screen */
            clearScreen(current);
            initLinenoiseLine(current);  
            /* Force recalc of window size for serial terminals */
            current->cols = 0;
            refreshLine(current->prompt, current);
            break;
        default:
            /* Only tab is allowed without ^V */
            if (c == '\t' || c >= ' ') {
              eraseEol(current);
                if (insert_char(current, current->pos, c) == 1) {
                    refreshLine(current->prompt, current);
                }
            }
            break;
        }
    }
    return current->len;
}

int linenoiseColumns(void)
{
    struct current current;
    enableRawMode (&current);
    getWindowSize (&current);
    disableRawMode (&current);
    return current.cols;
}

char *linenoise(const char *prompt)
{
    size_t count;
    struct current current;
    char buf[LINENOISE_MAX_LINE];

    if (enableRawMode(&current) == -1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            return NULL;
        }
        count = strlen(buf);
        if (count && buf[count-1] == '\n') {
            count--;
            buf[count] = '\0';
        }
    }
    else
    {
        current.buf = buf;
        current.bufmax = sizeof(buf);
        current.len = 0;
        current.chars = 0;
        current.pos = 0;
        current.prompt = prompt;
        current.capture = NULL;

        initLinenoiseLine(&current);
        count = linenoiseEdit(&current);

        disableRawMode(&current);
        printf("\n");

        free(current.capture);
        if (count == -1) {
            return NULL;
        }
    }
    return strdup(buf);
}

/* Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line) {
    char *linecopy;

    if (history_max_len == 0) return 0;
    if (history == NULL) {
        history = (char **)malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }

    /* do not insert duplicate lines into history */
    if (history_len > 0 && strcmp(line, history[history_len - 1]) == 0) {
        return 0;
    }

    linecopy = strdup(line);
    if (!linecopy) return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history,history+1,sizeof(char*)*(history_max_len-1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
}

int linenoiseHistoryGetMaxLen(void) {
    return history_max_len;
}

int linenoiseHistorySetMaxLen(int len) {
    char **newHistory;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        newHistory = (char **)malloc(sizeof(char*)*len);
        if (newHistory == NULL) return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++) free(history[j]);
            tocopy = len;
        }
        memset(newHistory,0,sizeof(char*)*len);
        memcpy(newHistory,history+(history_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = newHistory;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char *filename) {
    FILE *fp = fopen(filename,"w");
    int j;

    if (fp == NULL) return -1;
    for (j = 0; j < history_len; j++) {
        const char *str = history[j];
        /* Need to encode backslash, nl and cr */
        while (*str) {
            if (*str == '\\') {
                fputs("\\\\", fp);
            }
            else if (*str == '\n') {
                fputs("\\n", fp);
            }
            else if (*str == '\r') {
                fputs("\\r", fp);
            }
            else {
                fputc(*str, fp);
            }
            str++;
        }
        fputc('\n', fp);
    }

    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const char *filename) {
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
        char *src, *dest;

        /* Decode backslash escaped values */
        for (src = dest = buf; *src; src++) {
            char ch = *src;

            if (ch == '\\') {
                src++;
                if (*src == 'n') {
                    ch = '\n';
                }
                else if (*src == 'r') {
                    ch = '\r';
                } else {
                    ch = *src;
                }
            }
            *dest++ = ch;
        }
        /* Remove trailing newline */
        if (dest != buf && (dest[-1] == '\n' || dest[-1] == '\r')) {
            dest--;
        }
        *dest = 0;

        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}

/* Provide access to the history buffer.
 *
 * If 'len' is not NULL, the length is stored in *len.
 */
char **linenoiseHistory(int *len) {
    if (len) {
        *len = history_len;
    }
    return history;
}
