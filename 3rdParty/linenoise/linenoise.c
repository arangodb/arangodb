/* linenoise.c -- VERSION 1.0
 *
 * Guerrilla line editing library against the idea that a line editing lib
 * needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
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
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
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
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "linenoise.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static linenoiseCompletionCallback *completionCallback = NULL;

static struct termios orig_termios; /* In order to restore at exit.*/
static int rawmode = 0; /* For atexit() function to check if restore is needed*/
static int mlmode = 0;  /* Multi line mode. Default is single line. */
static int atexit_registered = 0; /* Register atexit just 1 time. */
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

/* The linenoiseState structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
struct linenoiseState {
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldcolpos;   /* Previous refresh cursor column position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t maxrows;     /* Maximum num of rows used so far (multiline mode) */
    int history_index;  /* The history index we are currently editing. */
};

enum KEY_ACTION{
	KEY_NULL = 0,	    /* NULL */
	CTRL_A = 1,         /* Ctrl+a */
	CTRL_B = 2,         /* Ctrl-b */
	CTRL_C = 3,         /* Ctrl-c */
	CTRL_D = 4,         /* Ctrl-d */
	CTRL_E = 5,         /* Ctrl-e */
	CTRL_F = 6,         /* Ctrl-f */
	CTRL_H = 8,         /* Ctrl-h */
	TAB = 9,            /* Tab */
	CTRL_K = 11,        /* Ctrl+k */
	CTRL_L = 12,        /* Ctrl+l */
	ENTER = 13,         /* Enter */
	CTRL_N = 14,        /* Ctrl-n */
	CTRL_P = 16,        /* Ctrl-p */
	CTRL_T = 20,        /* Ctrl-t */
	CTRL_U = 21,        /* Ctrl+u */
	CTRL_W = 23,        /* Ctrl+w */
	ESC = 27,           /* Escape */
	BACKSPACE =  127    /* Backspace */
};

static void linenoiseAtExit(void);
int linenoiseHistoryAdd(const char *line);
static void refreshLine(struct linenoiseState *l);

/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)l->len,(int)l->pos,(int)l->oldcolpos,plen,rows,rpos, \
            (int)l->maxrows,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

/* ============================ UTF8 utilities ============================== */

static unsigned long unicodeWideCharTable[][2] = {
    { 0x1100, 0x115F }, { 0x2329, 0x232A }, { 0x2E80, 0x2E99, }, { 0x2E9B, 0x2EF3, },
    { 0x2F00, 0x2FD5, }, { 0x2FF0, 0x2FFB, }, { 0x3000, 0x303E, }, { 0x3041, 0x3096, },
    { 0x3099, 0x30FF, }, { 0x3105, 0x312D, }, { 0x3131, 0x318E, }, { 0x3190, 0x31BA, },
    { 0x31C0, 0x31E3, }, { 0x31F0, 0x321E, }, { 0x3220, 0x3247, }, { 0x3250, 0x4DBF, },
    { 0x4E00, 0xA48C, }, { 0xA490, 0xA4C6, }, { 0xA960, 0xA97C, }, { 0xAC00, 0xD7A3, },
    { 0xF900, 0xFAFF, }, { 0xFE10, 0xFE19, }, { 0xFE30, 0xFE52, }, { 0xFE54, 0xFE66, },
    { 0xFE68, 0xFE6B, }, { 0xFF01, 0xFFE6, },
    { 0x1B000, 0x1B001, }, { 0x1F200, 0x1F202, }, { 0x1F210, 0x1F23A, },
    { 0x1F240, 0x1F248, }, { 0x1F250, 0x1F251, }, { 0x20000, 0x3FFFD, },
};

static size_t unicodeWideCharTableSize = sizeof(unicodeWideCharTable) / sizeof(unicodeWideCharTable[0]);

static int unicodeIsWideChar(unsigned long cp)
{
    size_t i;
    for (i = 0; i < unicodeWideCharTableSize; i++) {
        if (unicodeWideCharTable[i][0] <= cp && cp <= unicodeWideCharTable[i][1]) {
            return 1;
        }
    }
    return 0;
}

static unsigned long unicodeCombiningCharTable[] = {
    0x0300,0x0301,0x0302,0x0303,0x0304,0x0305,0x0306,0x0307,
    0x0308,0x0309,0x030A,0x030B,0x030C,0x030D,0x030E,0x030F,
    0x0310,0x0311,0x0312,0x0313,0x0314,0x0315,0x0316,0x0317,
    0x0318,0x0319,0x031A,0x031B,0x031C,0x031D,0x031E,0x031F,
    0x0320,0x0321,0x0322,0x0323,0x0324,0x0325,0x0326,0x0327,
    0x0328,0x0329,0x032A,0x032B,0x032C,0x032D,0x032E,0x032F,
    0x0330,0x0331,0x0332,0x0333,0x0334,0x0335,0x0336,0x0337,
    0x0338,0x0339,0x033A,0x033B,0x033C,0x033D,0x033E,0x033F,
    0x0340,0x0341,0x0342,0x0343,0x0344,0x0345,0x0346,0x0347,
    0x0348,0x0349,0x034A,0x034B,0x034C,0x034D,0x034E,0x034F,
    0x0350,0x0351,0x0352,0x0353,0x0354,0x0355,0x0356,0x0357,
    0x0358,0x0359,0x035A,0x035B,0x035C,0x035D,0x035E,0x035F,
    0x0360,0x0361,0x0362,0x0363,0x0364,0x0365,0x0366,0x0367,
    0x0368,0x0369,0x036A,0x036B,0x036C,0x036D,0x036E,0x036F,
    0x0483,0x0484,0x0485,0x0486,0x0487,0x0591,0x0592,0x0593,
    0x0594,0x0595,0x0596,0x0597,0x0598,0x0599,0x059A,0x059B,
    0x059C,0x059D,0x059E,0x059F,0x05A0,0x05A1,0x05A2,0x05A3,
    0x05A4,0x05A5,0x05A6,0x05A7,0x05A8,0x05A9,0x05AA,0x05AB,
    0x05AC,0x05AD,0x05AE,0x05AF,0x05B0,0x05B1,0x05B2,0x05B3,
    0x05B4,0x05B5,0x05B6,0x05B7,0x05B8,0x05B9,0x05BA,0x05BB,
    0x05BC,0x05BD,0x05BF,0x05C1,0x05C2,0x05C4,0x05C5,0x05C7,
    0x0610,0x0611,0x0612,0x0613,0x0614,0x0615,0x0616,0x0617,
    0x0618,0x0619,0x061A,0x064B,0x064C,0x064D,0x064E,0x064F,
    0x0650,0x0651,0x0652,0x0653,0x0654,0x0655,0x0656,0x0657,
    0x0658,0x0659,0x065A,0x065B,0x065C,0x065D,0x065E,0x065F,
    0x0670,0x06D6,0x06D7,0x06D8,0x06D9,0x06DA,0x06DB,0x06DC,
    0x06DF,0x06E0,0x06E1,0x06E2,0x06E3,0x06E4,0x06E7,0x06E8,
    0x06EA,0x06EB,0x06EC,0x06ED,0x0711,0x0730,0x0731,0x0732,
    0x0733,0x0734,0x0735,0x0736,0x0737,0x0738,0x0739,0x073A,
    0x073B,0x073C,0x073D,0x073E,0x073F,0x0740,0x0741,0x0742,
    0x0743,0x0744,0x0745,0x0746,0x0747,0x0748,0x0749,0x074A,
    0x07A6,0x07A7,0x07A8,0x07A9,0x07AA,0x07AB,0x07AC,0x07AD,
    0x07AE,0x07AF,0x07B0,0x07EB,0x07EC,0x07ED,0x07EE,0x07EF,
    0x07F0,0x07F1,0x07F2,0x07F3,0x0816,0x0817,0x0818,0x0819,
    0x081B,0x081C,0x081D,0x081E,0x081F,0x0820,0x0821,0x0822,
    0x0823,0x0825,0x0826,0x0827,0x0829,0x082A,0x082B,0x082C,
    0x082D,0x0859,0x085A,0x085B,0x08E3,0x08E4,0x08E5,0x08E6,
    0x08E7,0x08E8,0x08E9,0x08EA,0x08EB,0x08EC,0x08ED,0x08EE,
    0x08EF,0x08F0,0x08F1,0x08F2,0x08F3,0x08F4,0x08F5,0x08F6,
    0x08F7,0x08F8,0x08F9,0x08FA,0x08FB,0x08FC,0x08FD,0x08FE,
    0x08FF,0x0900,0x0901,0x0902,0x093A,0x093C,0x0941,0x0942,
    0x0943,0x0944,0x0945,0x0946,0x0947,0x0948,0x094D,0x0951,
    0x0952,0x0953,0x0954,0x0955,0x0956,0x0957,0x0962,0x0963,
    0x0981,0x09BC,0x09C1,0x09C2,0x09C3,0x09C4,0x09CD,0x09E2,
    0x09E3,0x0A01,0x0A02,0x0A3C,0x0A41,0x0A42,0x0A47,0x0A48,
    0x0A4B,0x0A4C,0x0A4D,0x0A51,0x0A70,0x0A71,0x0A75,0x0A81,
    0x0A82,0x0ABC,0x0AC1,0x0AC2,0x0AC3,0x0AC4,0x0AC5,0x0AC7,
    0x0AC8,0x0ACD,0x0AE2,0x0AE3,0x0B01,0x0B3C,0x0B3F,0x0B41,
    0x0B42,0x0B43,0x0B44,0x0B4D,0x0B56,0x0B62,0x0B63,0x0B82,
    0x0BC0,0x0BCD,0x0C00,0x0C3E,0x0C3F,0x0C40,0x0C46,0x0C47,
    0x0C48,0x0C4A,0x0C4B,0x0C4C,0x0C4D,0x0C55,0x0C56,0x0C62,
    0x0C63,0x0C81,0x0CBC,0x0CBF,0x0CC6,0x0CCC,0x0CCD,0x0CE2,
    0x0CE3,0x0D01,0x0D41,0x0D42,0x0D43,0x0D44,0x0D4D,0x0D62,
    0x0D63,0x0DCA,0x0DD2,0x0DD3,0x0DD4,0x0DD6,0x0E31,0x0E34,
    0x0E35,0x0E36,0x0E37,0x0E38,0x0E39,0x0E3A,0x0E47,0x0E48,
    0x0E49,0x0E4A,0x0E4B,0x0E4C,0x0E4D,0x0E4E,0x0EB1,0x0EB4,
    0x0EB5,0x0EB6,0x0EB7,0x0EB8,0x0EB9,0x0EBB,0x0EBC,0x0EC8,
    0x0EC9,0x0ECA,0x0ECB,0x0ECC,0x0ECD,0x0F18,0x0F19,0x0F35,
    0x0F37,0x0F39,0x0F71,0x0F72,0x0F73,0x0F74,0x0F75,0x0F76,
    0x0F77,0x0F78,0x0F79,0x0F7A,0x0F7B,0x0F7C,0x0F7D,0x0F7E,
    0x0F80,0x0F81,0x0F82,0x0F83,0x0F84,0x0F86,0x0F87,0x0F8D,
    0x0F8E,0x0F8F,0x0F90,0x0F91,0x0F92,0x0F93,0x0F94,0x0F95,
    0x0F96,0x0F97,0x0F99,0x0F9A,0x0F9B,0x0F9C,0x0F9D,0x0F9E,
    0x0F9F,0x0FA0,0x0FA1,0x0FA2,0x0FA3,0x0FA4,0x0FA5,0x0FA6,
    0x0FA7,0x0FA8,0x0FA9,0x0FAA,0x0FAB,0x0FAC,0x0FAD,0x0FAE,
    0x0FAF,0x0FB0,0x0FB1,0x0FB2,0x0FB3,0x0FB4,0x0FB5,0x0FB6,
    0x0FB7,0x0FB8,0x0FB9,0x0FBA,0x0FBB,0x0FBC,0x0FC6,0x102D,
    0x102E,0x102F,0x1030,0x1032,0x1033,0x1034,0x1035,0x1036,
    0x1037,0x1039,0x103A,0x103D,0x103E,0x1058,0x1059,0x105E,
    0x105F,0x1060,0x1071,0x1072,0x1073,0x1074,0x1082,0x1085,
    0x1086,0x108D,0x109D,0x135D,0x135E,0x135F,0x1712,0x1713,
    0x1714,0x1732,0x1733,0x1734,0x1752,0x1753,0x1772,0x1773,
    0x17B4,0x17B5,0x17B7,0x17B8,0x17B9,0x17BA,0x17BB,0x17BC,
    0x17BD,0x17C6,0x17C9,0x17CA,0x17CB,0x17CC,0x17CD,0x17CE,
    0x17CF,0x17D0,0x17D1,0x17D2,0x17D3,0x17DD,0x180B,0x180C,
    0x180D,0x18A9,0x1920,0x1921,0x1922,0x1927,0x1928,0x1932,
    0x1939,0x193A,0x193B,0x1A17,0x1A18,0x1A1B,0x1A56,0x1A58,
    0x1A59,0x1A5A,0x1A5B,0x1A5C,0x1A5D,0x1A5E,0x1A60,0x1A62,
    0x1A65,0x1A66,0x1A67,0x1A68,0x1A69,0x1A6A,0x1A6B,0x1A6C,
    0x1A73,0x1A74,0x1A75,0x1A76,0x1A77,0x1A78,0x1A79,0x1A7A,
    0x1A7B,0x1A7C,0x1A7F,0x1AB0,0x1AB1,0x1AB2,0x1AB3,0x1AB4,
    0x1AB5,0x1AB6,0x1AB7,0x1AB8,0x1AB9,0x1ABA,0x1ABB,0x1ABC,
    0x1ABD,0x1B00,0x1B01,0x1B02,0x1B03,0x1B34,0x1B36,0x1B37,
    0x1B38,0x1B39,0x1B3A,0x1B3C,0x1B42,0x1B6B,0x1B6C,0x1B6D,
    0x1B6E,0x1B6F,0x1B70,0x1B71,0x1B72,0x1B73,0x1B80,0x1B81,
    0x1BA2,0x1BA3,0x1BA4,0x1BA5,0x1BA8,0x1BA9,0x1BAB,0x1BAC,
    0x1BAD,0x1BE6,0x1BE8,0x1BE9,0x1BED,0x1BEF,0x1BF0,0x1BF1,
    0x1C2C,0x1C2D,0x1C2E,0x1C2F,0x1C30,0x1C31,0x1C32,0x1C33,
    0x1C36,0x1C37,0x1CD0,0x1CD1,0x1CD2,0x1CD4,0x1CD5,0x1CD6,
    0x1CD7,0x1CD8,0x1CD9,0x1CDA,0x1CDB,0x1CDC,0x1CDD,0x1CDE,
    0x1CDF,0x1CE0,0x1CE2,0x1CE3,0x1CE4,0x1CE5,0x1CE6,0x1CE7,
    0x1CE8,0x1CED,0x1CF4,0x1CF8,0x1CF9,0x1DC0,0x1DC1,0x1DC2,
    0x1DC3,0x1DC4,0x1DC5,0x1DC6,0x1DC7,0x1DC8,0x1DC9,0x1DCA,
    0x1DCB,0x1DCC,0x1DCD,0x1DCE,0x1DCF,0x1DD0,0x1DD1,0x1DD2,
    0x1DD3,0x1DD4,0x1DD5,0x1DD6,0x1DD7,0x1DD8,0x1DD9,0x1DDA,
    0x1DDB,0x1DDC,0x1DDD,0x1DDE,0x1DDF,0x1DE0,0x1DE1,0x1DE2,
    0x1DE3,0x1DE4,0x1DE5,0x1DE6,0x1DE7,0x1DE8,0x1DE9,0x1DEA,
    0x1DEB,0x1DEC,0x1DED,0x1DEE,0x1DEF,0x1DF0,0x1DF1,0x1DF2,
    0x1DF3,0x1DF4,0x1DF5,0x1DFC,0x1DFD,0x1DFE,0x1DFF,0x20D0,
    0x20D1,0x20D2,0x20D3,0x20D4,0x20D5,0x20D6,0x20D7,0x20D8,
    0x20D9,0x20DA,0x20DB,0x20DC,0x20E1,0x20E5,0x20E6,0x20E7,
    0x20E8,0x20E9,0x20EA,0x20EB,0x20EC,0x20ED,0x20EE,0x20EF,
    0x20F0,0x2CEF,0x2CF0,0x2CF1,0x2D7F,0x2DE0,0x2DE1,0x2DE2,
    0x2DE3,0x2DE4,0x2DE5,0x2DE6,0x2DE7,0x2DE8,0x2DE9,0x2DEA,
    0x2DEB,0x2DEC,0x2DED,0x2DEE,0x2DEF,0x2DF0,0x2DF1,0x2DF2,
    0x2DF3,0x2DF4,0x2DF5,0x2DF6,0x2DF7,0x2DF8,0x2DF9,0x2DFA,
    0x2DFB,0x2DFC,0x2DFD,0x2DFE,0x2DFF,0x302A,0x302B,0x302C,
    0x302D,0x3099,0x309A,0xA66F,0xA674,0xA675,0xA676,0xA677,
    0xA678,0xA679,0xA67A,0xA67B,0xA67C,0xA67D,0xA69E,0xA69F,
    0xA6F0,0xA6F1,0xA802,0xA806,0xA80B,0xA825,0xA826,0xA8C4,
    0xA8E0,0xA8E1,0xA8E2,0xA8E3,0xA8E4,0xA8E5,0xA8E6,0xA8E7,
    0xA8E8,0xA8E9,0xA8EA,0xA8EB,0xA8EC,0xA8ED,0xA8EE,0xA8EF,
    0xA8F0,0xA8F1,0xA926,0xA927,0xA928,0xA929,0xA92A,0xA92B,
    0xA92C,0xA92D,0xA947,0xA948,0xA949,0xA94A,0xA94B,0xA94C,
    0xA94D,0xA94E,0xA94F,0xA950,0xA951,0xA980,0xA981,0xA982,
    0xA9B3,0xA9B6,0xA9B7,0xA9B8,0xA9B9,0xA9BC,0xA9E5,0xAA29,
    0xAA2A,0xAA2B,0xAA2C,0xAA2D,0xAA2E,0xAA31,0xAA32,0xAA35,
    0xAA36,0xAA43,0xAA4C,0xAA7C,0xAAB0,0xAAB2,0xAAB3,0xAAB4,
    0xAAB7,0xAAB8,0xAABE,0xAABF,0xAAC1,0xAAEC,0xAAED,0xAAF6,
    0xABE5,0xABE8,0xABED,0xFB1E,0xFE00,0xFE01,0xFE02,0xFE03,
    0xFE04,0xFE05,0xFE06,0xFE07,0xFE08,0xFE09,0xFE0A,0xFE0B,
    0xFE0C,0xFE0D,0xFE0E,0xFE0F,0xFE20,0xFE21,0xFE22,0xFE23,
    0xFE24,0xFE25,0xFE26,0xFE27,0xFE28,0xFE29,0xFE2A,0xFE2B,
    0xFE2C,0xFE2D,0xFE2E,0xFE2F,
    0x101FD,0x102E0,0x10376,0x10377,0x10378,0x10379,0x1037A,0x10A01,
    0x10A02,0x10A03,0x10A05,0x10A06,0x10A0C,0x10A0D,0x10A0E,0x10A0F,
    0x10A38,0x10A39,0x10A3A,0x10A3F,0x10AE5,0x10AE6,0x11001,0x11038,
    0x11039,0x1103A,0x1103B,0x1103C,0x1103D,0x1103E,0x1103F,0x11040,
    0x11041,0x11042,0x11043,0x11044,0x11045,0x11046,0x1107F,0x11080,
    0x11081,0x110B3,0x110B4,0x110B5,0x110B6,0x110B9,0x110BA,0x11100,
    0x11101,0x11102,0x11127,0x11128,0x11129,0x1112A,0x1112B,0x1112D,
    0x1112E,0x1112F,0x11130,0x11131,0x11132,0x11133,0x11134,0x11173,
    0x11180,0x11181,0x111B6,0x111B7,0x111B8,0x111B9,0x111BA,0x111BB,
    0x111BC,0x111BD,0x111BE,0x111CA,0x111CB,0x111CC,0x1122F,0x11230,
    0x11231,0x11234,0x11236,0x11237,0x112DF,0x112E3,0x112E4,0x112E5,
    0x112E6,0x112E7,0x112E8,0x112E9,0x112EA,0x11300,0x11301,0x1133C,
    0x11340,0x11366,0x11367,0x11368,0x11369,0x1136A,0x1136B,0x1136C,
    0x11370,0x11371,0x11372,0x11373,0x11374,0x114B3,0x114B4,0x114B5,
    0x114B6,0x114B7,0x114B8,0x114BA,0x114BF,0x114C0,0x114C2,0x114C3,
    0x115B2,0x115B3,0x115B4,0x115B5,0x115BC,0x115BD,0x115BF,0x115C0,
    0x115DC,0x115DD,0x11633,0x11634,0x11635,0x11636,0x11637,0x11638,
    0x11639,0x1163A,0x1163D,0x1163F,0x11640,0x116AB,0x116AD,0x116B0,
    0x116B1,0x116B2,0x116B3,0x116B4,0x116B5,0x116B7,0x1171D,0x1171E,
    0x1171F,0x11722,0x11723,0x11724,0x11725,0x11727,0x11728,0x11729,
    0x1172A,0x1172B,0x16AF0,0x16AF1,0x16AF2,0x16AF3,0x16AF4,0x16B30,
    0x16B31,0x16B32,0x16B33,0x16B34,0x16B35,0x16B36,0x16F8F,0x16F90,
    0x16F91,0x16F92,0x1BC9D,0x1BC9E,0x1D167,0x1D168,0x1D169,0x1D17B,
    0x1D17C,0x1D17D,0x1D17E,0x1D17F,0x1D180,0x1D181,0x1D182,0x1D185,
    0x1D186,0x1D187,0x1D188,0x1D189,0x1D18A,0x1D18B,0x1D1AA,0x1D1AB,
    0x1D1AC,0x1D1AD,0x1D242,0x1D243,0x1D244,0x1DA00,0x1DA01,0x1DA02,
    0x1DA03,0x1DA04,0x1DA05,0x1DA06,0x1DA07,0x1DA08,0x1DA09,0x1DA0A,
    0x1DA0B,0x1DA0C,0x1DA0D,0x1DA0E,0x1DA0F,0x1DA10,0x1DA11,0x1DA12,
    0x1DA13,0x1DA14,0x1DA15,0x1DA16,0x1DA17,0x1DA18,0x1DA19,0x1DA1A,
    0x1DA1B,0x1DA1C,0x1DA1D,0x1DA1E,0x1DA1F,0x1DA20,0x1DA21,0x1DA22,
    0x1DA23,0x1DA24,0x1DA25,0x1DA26,0x1DA27,0x1DA28,0x1DA29,0x1DA2A,
    0x1DA2B,0x1DA2C,0x1DA2D,0x1DA2E,0x1DA2F,0x1DA30,0x1DA31,0x1DA32,
    0x1DA33,0x1DA34,0x1DA35,0x1DA36,0x1DA3B,0x1DA3C,0x1DA3D,0x1DA3E,
    0x1DA3F,0x1DA40,0x1DA41,0x1DA42,0x1DA43,0x1DA44,0x1DA45,0x1DA46,
    0x1DA47,0x1DA48,0x1DA49,0x1DA4A,0x1DA4B,0x1DA4C,0x1DA4D,0x1DA4E,
    0x1DA4F,0x1DA50,0x1DA51,0x1DA52,0x1DA53,0x1DA54,0x1DA55,0x1DA56,
    0x1DA57,0x1DA58,0x1DA59,0x1DA5A,0x1DA5B,0x1DA5C,0x1DA5D,0x1DA5E,
    0x1DA5F,0x1DA60,0x1DA61,0x1DA62,0x1DA63,0x1DA64,0x1DA65,0x1DA66,
    0x1DA67,0x1DA68,0x1DA69,0x1DA6A,0x1DA6B,0x1DA6C,0x1DA75,0x1DA84,
    0x1DA9B,0x1DA9C,0x1DA9D,0x1DA9E,0x1DA9F,0x1DAA1,0x1DAA2,0x1DAA3,
    0x1DAA4,0x1DAA5,0x1DAA6,0x1DAA7,0x1DAA8,0x1DAA9,0x1DAAA,0x1DAAB,
    0x1DAAC,0x1DAAD,0x1DAAE,0x1DAAF,0x1E8D0,0x1E8D1,0x1E8D2,0x1E8D3,
    0x1E8D4,0x1E8D5,0x1E8D6,0xE0100,0xE0101,0xE0102,0xE0103,0xE0104,
    0xE0105,0xE0106,0xE0107,0xE0108,0xE0109,0xE010A,0xE010B,0xE010C,
    0xE010D,0xE010E,0xE010F,0xE0110,0xE0111,0xE0112,0xE0113,0xE0114,
    0xE0115,0xE0116,0xE0117,0xE0118,0xE0119,0xE011A,0xE011B,0xE011C,
    0xE011D,0xE011E,0xE011F,0xE0120,0xE0121,0xE0122,0xE0123,0xE0124,
    0xE0125,0xE0126,0xE0127,0xE0128,0xE0129,0xE012A,0xE012B,0xE012C,
    0xE012D,0xE012E,0xE012F,0xE0130,0xE0131,0xE0132,0xE0133,0xE0134,
    0xE0135,0xE0136,0xE0137,0xE0138,0xE0139,0xE013A,0xE013B,0xE013C,
    0xE013D,0xE013E,0xE013F,0xE0140,0xE0141,0xE0142,0xE0143,0xE0144,
    0xE0145,0xE0146,0xE0147,0xE0148,0xE0149,0xE014A,0xE014B,0xE014C,
    0xE014D,0xE014E,0xE014F,0xE0150,0xE0151,0xE0152,0xE0153,0xE0154,
    0xE0155,0xE0156,0xE0157,0xE0158,0xE0159,0xE015A,0xE015B,0xE015C,
    0xE015D,0xE015E,0xE015F,0xE0160,0xE0161,0xE0162,0xE0163,0xE0164,
    0xE0165,0xE0166,0xE0167,0xE0168,0xE0169,0xE016A,0xE016B,0xE016C,
    0xE016D,0xE016E,0xE016F,0xE0170,0xE0171,0xE0172,0xE0173,0xE0174,
    0xE0175,0xE0176,0xE0177,0xE0178,0xE0179,0xE017A,0xE017B,0xE017C,
    0xE017D,0xE017E,0xE017F,0xE0180,0xE0181,0xE0182,0xE0183,0xE0184,
    0xE0185,0xE0186,0xE0187,0xE0188,0xE0189,0xE018A,0xE018B,0xE018C,
    0xE018D,0xE018E,0xE018F,0xE0190,0xE0191,0xE0192,0xE0193,0xE0194,
    0xE0195,0xE0196,0xE0197,0xE0198,0xE0199,0xE019A,0xE019B,0xE019C,
    0xE019D,0xE019E,0xE019F,0xE01A0,0xE01A1,0xE01A2,0xE01A3,0xE01A4,
    0xE01A5,0xE01A6,0xE01A7,0xE01A8,0xE01A9,0xE01AA,0xE01AB,0xE01AC,
    0xE01AD,0xE01AE,0xE01AF,0xE01B0,0xE01B1,0xE01B2,0xE01B3,0xE01B4,
    0xE01B5,0xE01B6,0xE01B7,0xE01B8,0xE01B9,0xE01BA,0xE01BB,0xE01BC,
    0xE01BD,0xE01BE,0xE01BF,0xE01C0,0xE01C1,0xE01C2,0xE01C3,0xE01C4,
    0xE01C5,0xE01C6,0xE01C7,0xE01C8,0xE01C9,0xE01CA,0xE01CB,0xE01CC,
    0xE01CD,0xE01CE,0xE01CF,0xE01D0,0xE01D1,0xE01D2,0xE01D3,0xE01D4,
    0xE01D5,0xE01D6,0xE01D7,0xE01D8,0xE01D9,0xE01DA,0xE01DB,0xE01DC,
    0xE01DD,0xE01DE,0xE01DF,0xE01E0,0xE01E1,0xE01E2,0xE01E3,0xE01E4,
    0xE01E5,0xE01E6,0xE01E7,0xE01E8,0xE01E9,0xE01EA,0xE01EB,0xE01EC,
    0xE01ED,0xE01EE,0xE01EF,
};

static unsigned long unicodeCombiningCharTableSize = sizeof(unicodeCombiningCharTable) / sizeof(unicodeCombiningCharTable[0]);

static int unicodeIsCombiningChar(unsigned long cp)
{
    size_t i;
    for (i = 0; i < unicodeCombiningCharTableSize; i++) {
        if (unicodeCombiningCharTable[i] == cp) {
            return 1;
        }
    }
    return 0;
}

/* Get length of previous UTF8 character
 */
static size_t unicodePrevUTF8CharLen(char* buf, int pos)
{
    int end = pos--;
    while (pos >= 0 && ((unsigned char)buf[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return end - pos;
}

/* Get length of previous UTF8 character
 */
static size_t unicodeUTF8CharLen(char* buf, size_t buf_len, size_t pos)
{
    if (pos == buf_len) { return 0; }
    unsigned char ch = buf[pos];
    if (ch < 0x80) { return 1; }
    else if (ch < 0xE0) { return 2; }
    else if (ch < 0xF0) { return 3; }
    else { return 4; }
}

/* Convert UTF8 to Unicode code point
 */
static size_t unicodeUTF8CharToCodePoint(
   const char* buf,
   size_t      len,
   int*        cp)
{
    if (len) {
        unsigned char byte = buf[0];
        if ((byte & 0x80) == 0) {
            *cp = byte;
            return 1;
        } else if ((byte & 0xE0) == 0xC0) {
            if (len >= 2) {
                *cp = (((unsigned long)(buf[0] & 0x1F)) << 6) |
                       ((unsigned long)(buf[1] & 0x3F));
                return 2;
            }
        } else if ((byte & 0xF0) == 0xE0) {
            if (len >= 3) {
                *cp = (((unsigned long)(buf[0] & 0x0F)) << 12) |
                      (((unsigned long)(buf[1] & 0x3F)) << 6) |
                       ((unsigned long)(buf[2] & 0x3F));
                return 3;
            }
        } else if ((byte & 0xF8) == 0xF0) {
            if (len >= 4) {
                *cp = (((unsigned long)(buf[0] & 0x07)) << 18) |
                      (((unsigned long)(buf[1] & 0x3F)) << 12) |
                      (((unsigned long)(buf[2] & 0x3F)) << 6) |
                       ((unsigned long)(buf[3] & 0x3F));
                return 4;
            }
        }
    }
    return 0;
}

/* Get length of grapheme
 */
static size_t unicodeGraphemeLen(char* buf, size_t buf_len, size_t pos)
{
    if (pos == buf_len) {
        return 0;
    }
    size_t beg = pos;
    pos += unicodeUTF8CharLen(buf, buf_len, pos);
    while (pos < buf_len) {
        size_t len = unicodeUTF8CharLen(buf, buf_len, pos);
        int cp;
        unicodeUTF8CharToCodePoint(buf + pos, len, &cp);
        if (!unicodeIsCombiningChar(cp)) {
            return pos - beg;
        }
        pos += len;
    }
    return pos - beg;
}

/* Get length of previous grapheme
 */
static size_t unicodePrevGraphemeLen(char* buf, size_t pos)
{
    if (pos == 0) {
        return 0;
    }
    size_t end = pos;
    while (pos > 0) {
        size_t len = unicodePrevUTF8CharLen(buf, pos);
        pos -= len;
        int cp;
        unicodeUTF8CharToCodePoint(buf + pos, len, &cp);
        if (!unicodeIsCombiningChar(cp)) {
            return end - pos;
        }
    }
    return 0;
}

static int isAnsiEscape(const char* buf, size_t buf_len, size_t* len)
{
    if (buf_len > 2 && !memcmp("\033[", buf, 2)) {
        size_t off = 2;
        while (off < buf_len) {
            switch (buf[off++]) {
            case 'A': case 'B': case 'C': case 'D':
            case 'E': case 'F': case 'G': case 'H':
            case 'J': case 'K': case 'S': case 'T':
            case 'f': case 'm':
                *len = off;
                return 1;
            }
        }
    }
    return 0;
}

/* Get column position for the single line mode.
 */
static size_t unicodeColumnPos(const char* buf, size_t buf_len)
{
    size_t ret = 0;

    size_t off = 0;
    while (off < buf_len) {
        size_t len;
        if (isAnsiEscape(buf + off, buf_len - off, &len)) {
            off += len;
            continue;
        }

        int cp;
        len = unicodeUTF8CharToCodePoint(buf + off, buf_len - off, &cp);

        if (!unicodeIsCombiningChar(cp)) {
            ret += unicodeIsWideChar(cp) ? 2 : 1;
        }

        off += len;
    }

    return ret;
}

/* Get column position for the multi line mode.
 */
static size_t unicodeColumnPosForMultiLine(char* buf, size_t buf_len, size_t pos, size_t cols, size_t ini_pos)
{
    size_t ret = 0;
    size_t colwid = ini_pos;

    size_t off = 0;
    while (off < buf_len) {
        int cp;
        size_t len = unicodeUTF8CharToCodePoint(buf + off, buf_len - off, &cp);

        size_t wid = 0;
        if (!unicodeIsCombiningChar(cp)) {
            wid = unicodeIsWideChar(cp) ? 2 : 1;
        }

        int dif = (int)(colwid + wid) - (int)cols;
        if (dif > 0) {
            ret += dif;
            colwid = wid;
        } else if (dif == 0) {
            colwid = 0;
        } else {
            colwid += wid;
        }

        if (off >= pos) {
            break;
        }

        off += len;
        ret += wid;
    }

    return ret;
}

/* Read UTF8 character from file.
 */
static size_t unicodeReadUTF8Char(int fd, char* buf, int* cp)
{
    size_t nread = read(fd,&buf[0],1);

    if (nread <= 0) { return nread; }

    unsigned char byte = buf[0];

    if ((byte & 0x80) == 0) {
        ;
    } else if ((byte & 0xE0) == 0xC0) {
        nread = read(fd,&buf[1],1);
        if (nread <= 0) { return nread; }
    } else if ((byte & 0xF0) == 0xE0) {
        nread = read(fd,&buf[1],2);
        if (nread <= 0) { return nread; }
    } else if ((byte & 0xF8) == 0xF0) {
        nread = read(fd,&buf[1],3);
        if (nread <= 0) { return nread; }
    } else {
        return -1;
    }

    return unicodeUTF8CharToCodePoint(buf, 4, cp);
}

/* ======================= Low level terminal handling ====================== */

/* Set if to use or not the multi line mode. */
void linenoiseSetMultiLine(int ml) {
    mlmode = ml;
}

/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
static int isUnsupportedTerm(void) {
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return 0;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term,unsupported_term[j])) return 1;
    return 0;
}

/* Raw mode: 1960 magic shit. */
static int enableRawMode(int fd) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

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
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disableRawMode(int fd) {
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int getCursorPosition(int ifd, int ofd) {
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",&rows,&cols) != 2) return -1;
    return cols;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int getColumns(int ifd, int ofd) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = getCursorPosition(ifd,ofd);
        if (start == -1) goto failed;

        /* Go to right margin and get position. */
        if (write(ofd,"\x1b[999C",6) != 6) goto failed;
        cols = getCursorPosition(ifd,ofd);
        if (cols == -1) goto failed;

        /* Restore position. */
        if (cols > start) {
            char seq[32];
            snprintf(seq,32,"\x1b[%dD",cols-start);
            if (write(ofd,seq,strlen(seq)) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }

failed:
    return 80;
}

/* Clear the screen. Used to handle ctrl+l */
void linenoiseClearScreen(void) {
    if (write(STDOUT_FILENO,"\x1b[H\x1b[2J",7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void linenoiseBeep(void) {
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    if (lc->cvec != NULL)
        free(lc->cvec);
}

/* This is an helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
static int completeLine(struct linenoiseState *ls) {
    linenoiseCompletions lc = { 0, NULL };
    int nread, nwritten;
    int c = 0;
    char cbuf[4];

    completionCallback(ls->buf,&lc);
    if (lc.len == 0) {
        linenoiseBeep();
    } else {
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                struct linenoiseState saved = *ls;

                ls->len = ls->pos = strlen(lc.cvec[i]);
                ls->buf = lc.cvec[i];
                refreshLine(ls);
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->buf = saved.buf;
            } else {
                refreshLine(ls);
            }

            nread = unicodeReadUTF8Char(ls->ifd,cbuf,&c);
            if (nread <= 0) {
                freeCompletions(&lc);
                return -1;
            }

            switch(c) {
                case 9: /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) linenoiseBeep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.len) refreshLine(ls);
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.len) {
                        nwritten = snprintf(ls->buf,ls->buflen,"%s",lc.cvec[i]);
                        ls->len = ls->pos = nwritten;
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

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}

/* =========================== Line editing ================================= */

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
    char *b;
    int len;
};

static void abInit(struct abuf *ab) {
    ab->b = NULL;
    ab->len = 0;
}

static void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

static void abFree(struct abuf *ab) {
    free(ab->b);
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void refreshSingleLine(struct linenoiseState *l) {
    char seq[64];
    size_t pcolwid = unicodeColumnPos(l->prompt, strlen(l->prompt));
    int fd = l->ofd;
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;
    struct abuf ab;

    while((pcolwid+unicodeColumnPos(buf, pos)) >= l->cols) {
        int glen = unicodeGraphemeLen(buf, len, 0);
        buf += glen;
        len -= glen;
        pos -= glen;
    }
    while (pcolwid+unicodeColumnPos(buf, len) > l->cols) {
        len -= unicodePrevGraphemeLen(buf, len);
    }

    abInit(&ab);
    /* Cursor to left edge */
    snprintf(seq,64,"\r");
    abAppend(&ab,seq,strlen(seq));
    /* Write the prompt and the current buffer content */
    abAppend(&ab,l->prompt,strlen(l->prompt));
    abAppend(&ab,buf,len);
    /* Erase to right */
    snprintf(seq,64,"\x1b[0K");
    abAppend(&ab,seq,strlen(seq));
    /* Move cursor to original position. */
    snprintf(seq,64,"\r\x1b[%dC", (int)(unicodeColumnPos(buf, pos)+pcolwid));
    abAppend(&ab,seq,strlen(seq));
    if (write(fd,ab.b,ab.len) == -1) {} /* Can't recover from write error. */
    abFree(&ab);
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void refreshMultiLine(struct linenoiseState *l) {
    char seq[64];
    size_t pcolwid = unicodeColumnPos(l->prompt, strlen(l->prompt));
    int colpos = unicodeColumnPosForMultiLine(l->buf, l->len, l->len, l->cols, pcolwid);
    int colpos2; /* cursor column position. */
    int rows = (pcolwid+colpos+l->cols-1)/l->cols; /* rows used by current buf. */
    int rpos = (pcolwid+l->oldcolpos+l->cols)/l->cols; /* cursor relative row. */
    int rpos2; /* rpos after refresh. */
    int col; /* colum position, zero-based. */
    int old_rows = l->maxrows;
    int fd = l->ofd, j;
    struct abuf ab;

    /* Update maxrows if needed. */
    if (rows > (int)l->maxrows) l->maxrows = rows;

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    abInit(&ab);
    if (old_rows-rpos > 0) {
        lndebug("go down %d", old_rows-rpos);
        snprintf(seq,64,"\x1b[%dB", old_rows-rpos);
        abAppend(&ab,seq,strlen(seq));
    }

    /* Now for every row clear it, go up. */
    for (j = 0; j < old_rows-1; j++) {
        lndebug("clear+up");
        snprintf(seq,64,"\r\x1b[0K\x1b[1A");
        abAppend(&ab,seq,strlen(seq));
    }

    /* Clean the top line. */
    lndebug("clear");
    snprintf(seq,64,"\r\x1b[0K");
    abAppend(&ab,seq,strlen(seq));

    /* Write the prompt and the current buffer content */
    abAppend(&ab,l->prompt,strlen(l->prompt));
    abAppend(&ab,l->buf,l->len);

    /* Get text width to cursor position */
    colpos2 = unicodeColumnPosForMultiLine(l->buf, l->len, l->pos, l->cols, pcolwid);

    /* If we are at the very end of the screen with our prompt, we need to
     * emit a newline and move the prompt to the first column. */
    if (l->pos &&
        l->pos == l->len &&
        (colpos2+pcolwid) % l->cols == 0)
    {
        lndebug("<newline>");
        abAppend(&ab,"\n",1);
        snprintf(seq,64,"\r");
        abAppend(&ab,seq,strlen(seq));
        rows++;
        if (rows > (int)l->maxrows) l->maxrows = rows;
    }

    /* Move cursor to right position. */
    rpos2 = (pcolwid+colpos2+l->cols)/l->cols; /* current cursor relative row. */
    lndebug("rpos2 %d", rpos2);

    /* Go up till we reach the expected positon. */
    if (rows-rpos2 > 0) {
        lndebug("go-up %d", rows-rpos2);
        snprintf(seq,64,"\x1b[%dA", rows-rpos2);
        abAppend(&ab,seq,strlen(seq));
    }

    /* Set column. */
    col = (pcolwid + colpos2) % l->cols;
    lndebug("set col %d", 1+col);
    if (col)
        snprintf(seq,64,"\r\x1b[%dC", col);
    else
        snprintf(seq,64,"\r");
    abAppend(&ab,seq,strlen(seq));

    lndebug("\n");
    l->oldcolpos = colpos2;

    if (write(fd,ab.b,ab.len) == -1) {} /* Can't recover from write error. */
    abFree(&ab);
}

/* Calls the two low level functions refreshSingleLine() or
 * refreshMultiLine() according to the selected mode. */
static void refreshLine(struct linenoiseState *l) {
    if (mlmode)
        refreshMultiLine(l);
    else
        refreshSingleLine(l);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
int linenoiseEditInsert(struct linenoiseState *l, const char* cbuf, int clen) {
    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            memcpy(&l->buf[l->pos],cbuf,clen);
            l->pos+=clen;
            l->len+=clen;;
            l->buf[l->len] = '\0';
            if ((!mlmode && unicodeColumnPos(l->prompt,l->plen)+unicodeColumnPos(l->buf,l->len) < l->cols) /* || mlmode */) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (write(l->ofd,cbuf,clen) == -1) return -1;
            } else {
                refreshLine(l);
            }
        } else {
            memmove(l->buf+l->pos+clen,l->buf+l->pos,l->len-l->pos);
            memcpy(&l->buf[l->pos],cbuf,clen);
            l->pos+=clen;
            l->len+=clen;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    }
    return 0;
}

/* Move cursor on the left. */
void linenoiseEditMoveLeft(struct linenoiseState *l) {
    if (l->pos > 0) {
        l->pos -= unicodePrevGraphemeLen(l->buf, l->pos);
        refreshLine(l);
    }
}

/* Move cursor on the right. */
void linenoiseEditMoveRight(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos += unicodeGraphemeLen(l->buf, l->len, l->pos);
        refreshLine(l);
    }
}

/* Move cursor to the start of the line. */
void linenoiseEditMoveHome(struct linenoiseState *l) {
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}

/* Move cursor to the end of the line. */
void linenoiseEditMoveEnd(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
void linenoiseEditHistoryNext(struct linenoiseState *l, int dir) {
    if (history_len > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        free(history[history_len - 1 - l->history_index]);
        history[history_len - 1 - l->history_index] = strdup(l->buf);
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= history_len) {
            l->history_index = history_len-1;
            return;
        }
        strncpy(l->buf,history[history_len - 1 - l->history_index],l->buflen);
        l->buf[l->buflen-1] = '\0';
        l->len = l->pos = strlen(l->buf);
        refreshLine(l);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
void linenoiseEditDelete(struct linenoiseState *l) {
    if (l->len > 0 && l->pos < l->len) {
        int glen = unicodeGraphemeLen(l->buf,l->len,l->pos);
        memmove(l->buf+l->pos,l->buf+l->pos+glen,l->len-l->pos-glen);
        l->len-=glen;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Backspace implementation. */
void linenoiseEditBackspace(struct linenoiseState *l) {
    if (l->pos > 0 && l->len > 0) {
        int glen = unicodePrevGraphemeLen(l->buf,l->pos);
        memmove(l->buf+l->pos-glen,l->buf+l->pos,l->len-l->pos);
        l->pos-=glen;
        l->len-=glen;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Delete the previosu word, maintaining the cursor at the start of the
 * current word. */
void linenoiseEditDeletePrevWord(struct linenoiseState *l) {
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos--;
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos,l->buf+old_pos,l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user type enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer. */
static int linenoiseEdit(int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt)
{
    struct linenoiseState l;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l.ifd = stdin_fd;
    l.ofd = stdout_fd;
    l.buf = buf;
    l.buflen = buflen;
    l.prompt = prompt;
    l.plen = strlen(prompt);
    l.oldcolpos = l.pos = 0;
    l.len = 0;
    l.cols = getColumns(stdin_fd, stdout_fd);
    l.maxrows = 0;
    l.history_index = 0;

    /* Buffer starts empty. */
    l.buf[0] = '\0';
    l.buflen--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");

    if (write(l.ofd,prompt,l.plen) == -1) return -1;
    while(1) {
        int c;
        char cbuf[4];
        int nread;
        char seq[3];

        nread = unicodeReadUTF8Char(l.ifd,cbuf,&c);
        if (nread <= 0) return l.len;

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && completionCallback != NULL) {
            c = completeLine(&l);
            /* Return on errors */
            if (c < 0) return l.len;
            /* Read next character when 0 */
            if (c == 0) continue;
        }

        switch(c) {
        case ENTER:    /* enter */
            history_len--;
            free(history[history_len]);
            if (mlmode) linenoiseEditMoveEnd(&l);
            return (int)l.len;
        case CTRL_C:     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case BACKSPACE:   /* backspace */
        case 8:     /* ctrl-h */
            linenoiseEditBackspace(&l);
            break;
        case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
            if (l.len > 0) {
                linenoiseEditDelete(&l);
            } else {
                history_len--;
                free(history[history_len]);
                return -1;
            }
            break;
        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (l.pos > 0 && l.pos < l.len) {
                int aux = buf[l.pos-1];
                buf[l.pos-1] = buf[l.pos];
                buf[l.pos] = aux;
                if (l.pos != l.len-1) l.pos++;
                refreshLine(&l);
            }
            break;
        case CTRL_B:     /* ctrl-b */
            linenoiseEditMoveLeft(&l);
            break;
        case CTRL_F:     /* ctrl-f */
            linenoiseEditMoveRight(&l);
            break;
        case CTRL_P:    /* ctrl-p */
            linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N:    /* ctrl-n */
            linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_NEXT);
            break;
        case ESC:    /* escape sequence */
            /* Read the next two bytes representing the escape sequence.
             * Use two calls to handle slow terminals returning the two
             * chars at different times. */
            if (read(l.ifd,seq,1) == -1) break;
            if (read(l.ifd,seq+1,1) == -1) break;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(l.ifd,seq+2,1) == -1) break;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': /* Delete key. */
                            linenoiseEditDelete(&l);
                            break;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': /* Up */
                        linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        linenoiseEditHistoryNext(&l, LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        linenoiseEditMoveRight(&l);
                        break;
                    case 'D': /* Left */
                        linenoiseEditMoveLeft(&l);
                        break;
                    case 'H': /* Home */
                        linenoiseEditMoveHome(&l);
                        break;
                    case 'F': /* End*/
                        linenoiseEditMoveEnd(&l);
                        break;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': /* Home */
                    linenoiseEditMoveHome(&l);
                    break;
                case 'F': /* End*/
                    linenoiseEditMoveEnd(&l);
                    break;
                }
            }
            break;
        default:
            if (linenoiseEditInsert(&l,cbuf,nread)) return -1;
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            buf[0] = '\0';
            l.pos = l.len = 0;
            refreshLine(&l);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            buf[l.pos] = '\0';
            l.len = l.pos;
            refreshLine(&l);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            linenoiseEditMoveHome(&l);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            linenoiseEditMoveEnd(&l);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            linenoiseClearScreen();
            refreshLine(&l);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            linenoiseEditDeletePrevWord(&l);
            break;
        }
    }
    return l.len;
}

/* This special mode is used by linenoise in order to print scan codes
 * on screen for debugging / development purposes. It is implemented
 * by the linenoise_example program using the --keycodes option. */
void linenoisePrintKeyCodes(void) {
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
            "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (enableRawMode(STDIN_FILENO) == -1) return;
    memset(quit,' ',4);
    while(1) {
        char c;
        int nread;

        nread = read(STDIN_FILENO,&c,1);
        if (nread <= 0) continue;
        memmove(quit,quit+1,sizeof(quit)-1); /* shift string to left. */
        quit[sizeof(quit)-1] = c; /* Insert current char on the right. */
        if (memcmp(quit,"quit",sizeof(quit)) == 0) break;

        printf("'%c' %02x (%d) (type quit to exit)\n",
            isprint(c) ? c : '?', (int)c, (int)c);
        printf("\r"); /* Go left edge manually, we are in raw mode. */
        fflush(stdout);
    }
    disableRawMode(STDIN_FILENO);
}

/* This function calls the line editing function linenoiseEdit() using
 * the STDIN file descriptor set in raw mode. */
static int linenoiseRaw(char *buf, size_t buflen, const char *prompt) {
    int count;

    if (buflen == 0) {
        errno = EINVAL;
        return -1;
    }
    if (!isatty(STDIN_FILENO)) {
        /* Not a tty: read from file / pipe. */
        if (fgets(buf, buflen, stdin) == NULL) return -1;
        count = strlen(buf);
        if (count && buf[count-1] == '\n') {
            count--;
            buf[count] = '\0';
        }
    } else {
        /* Interactive editing. */
        if (enableRawMode(STDIN_FILENO) == -1) return -1;
        count = linenoiseEdit(STDIN_FILENO, STDOUT_FILENO, buf, buflen, prompt);
        disableRawMode(STDIN_FILENO);
        printf("\n");
    }
    return count;
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions. */
char *linenoise(const char *prompt) {
    char buf[LINENOISE_MAX_LINE];
    int count;

    if (isUnsupportedTerm()) {
        size_t len;

        printf("%s",prompt);
        fflush(stdout);
        if (fgets(buf,LINENOISE_MAX_LINE,stdin) == NULL) return NULL;
        len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return strdup(buf);
    } else {
        count = linenoiseRaw(buf,LINENOISE_MAX_LINE,prompt);
        if (count == -1) return NULL;
        return strdup(buf);
    }
}

/* ================================ History ================================= */

/* Free the history, but does not reset it. Only used when we have to
 * exit() to avoid memory leaks are reported by valgrind & co. */
static void freeHistory(void) {
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
    }
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
    freeHistory();
}

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line) {
    char *linecopy;

    if (history_max_len == 0) return 0;

    /* Initialization on first call. */
    if (history == NULL) {
        history = malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }

    /* Don't add duplicated lines. */
    if (history_len && !strcmp(history[history_len-1], line)) return 0;

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
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

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int linenoiseHistorySetMaxLen(int len) {
    char **new;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        new = malloc(sizeof(char*)*len);
        if (new == NULL) return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++) free(history[j]);
            tocopy = len;
        }
        memset(new,0,sizeof(char*)*len);
        memcpy(new,history+(history_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = new;
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
    for (j = 0; j < history_len; j++)
        fprintf(fp,"%s\n",history[j]);
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
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}
