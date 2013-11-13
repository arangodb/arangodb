/* input.c -- character input functions for readline. */

/* Copyright (C) 1994 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */
//#define READLINE_LIBRARY

#if defined (__TANDEM)
#  include <floss.h>
#endif

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#if defined (HAVE_SELECT)
#  if !defined (HAVE_SYS_SELECT_H) || !defined (M_UNIX)
#    include <sys/time.h>
#  endif
#endif /* HAVE_SELECT */
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#if defined (FIONREAD_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif /* !errno */

/* System-specific feature definitions and include files. */
#include "rldefs.h"
#include "rlmbutil.h"

/* Some standard library routines. */
#include "readline.h"

#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

/* What kind of non-blocking I/O do we have? */
#if !defined (O_NDELAY) && defined (O_NONBLOCK)
#  define O_NDELAY O_NONBLOCK	/* Posix style */
#endif

/* Non-null means it is a pointer to a function to run while waiting for
   character input. */
rl_hook_func_t *rl_event_hook = (rl_hook_func_t *)NULL;

rl_getc_func_t *rl_getc_function = rl_getc;

static int _keyboard_input_timeout = 100000;		/* 0.1 seconds; it's in usec */

static int ibuffer_space PARAMS((void));
static int rl_get_char PARAMS((int *));
static int rl_gather_tyi PARAMS((void));

/* **************************************************************** */
/*								    */
/*			Character Input Buffering       	    */
/*								    */
/* **************************************************************** */

static int pop_index, push_index;
static unsigned char ibuffer[512];
static int ibuffer_len = sizeof (ibuffer) - 1;

#define any_typein (push_index != pop_index)

int
_rl_any_typein ()
{
  return any_typein;
}

/* Return the amount of space available in the buffer for stuffing
   characters. */
static int
ibuffer_space ()
{
  if (pop_index > push_index)
    return (pop_index - push_index - 1);
  else
    return (ibuffer_len - (push_index - pop_index));
}

/* Get a key from the buffer of characters to be read.
   Return the key in KEY.
   Result is KEY if there was a key, or 0 if there wasn't. */
static int
rl_get_char (key)
     int *key;
{
  if (push_index == pop_index)
    return (0);

  *key = ibuffer[pop_index++];

  if (pop_index >= ibuffer_len)
    pop_index = 0;

  return (1);
}

/* Stuff KEY into the *front* of the input buffer.
   Returns non-zero if successful, zero if there is
   no space left in the buffer. */
int
_rl_unget_char (key)
     int key;
{
  if (ibuffer_space ())
    {
      pop_index--;
      if (pop_index < 0)
	pop_index = ibuffer_len - 1;
      ibuffer[pop_index] = key;
      return (1);
    }
  return (0);
}

int
_rl_pushed_input_available ()
{
  return (push_index != pop_index);
}

#ifndef _WIN32
/* If a character is available to be read, then read it and stuff it into
   IBUFFER.  Otherwise, just return.  Returns number of characters read
   (0 if none available) and -1 on error (EIO). */
static int
rl_gather_tyi ()
{
  int tty;
  register int tem, result;
  int chars_avail, k;
  char input;
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
  struct timeval timeout;
#endif

  tty = fileno (rl_instream);

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (tty, &readfds);
  FD_SET (tty, &exceptfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = _keyboard_input_timeout;
  result = select (tty + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout);
  if (result <= 0)
    return 0;	/* Nothing to read. */
#endif

  result = -1;
#if defined (FIONREAD)
  errno = 0;
  result = ioctl (tty, FIONREAD, &chars_avail);
  if (result == -1 && errno == EIO)
    return -1;
#endif

#if defined (O_NDELAY)
  if (result == -1)
    {
      tem = fcntl (tty, F_GETFL, 0);

      fcntl (tty, F_SETFL, (tem | O_NDELAY));
      chars_avail = read (tty, &input, 1);

      fcntl (tty, F_SETFL, tem);
      if (chars_avail == -1 && errno == EAGAIN)
	return 0;
      if (chars_avail == 0)	/* EOF */
	{
	  rl_stuff_char (EOF);
	  return (0);
	}
    }
#endif /* O_NDELAY */

  /* If there's nothing available, don't waste time trying to read
     something. */
  if (chars_avail <= 0)
    return 0;

  tem = ibuffer_space ();

  if (chars_avail > tem)
    chars_avail = tem;

  /* One cannot read all of the available input.  I can only read a single
     character at a time, or else programs which require input can be
     thwarted.  If the buffer is larger than one character, I lose.
     Damn! */
  if (tem < ibuffer_len)
    chars_avail = 0;

  if (result != -1)
    {
      while (chars_avail--)
	{
	  k = (*rl_getc_function) (rl_instream);
	  rl_stuff_char (k);
	  if (k == NEWLINE || k == RETURN)
	    break;
	}
    }
  else
    {
      if (chars_avail)
	rl_stuff_char (input);
    }

  return 1;
}
#endif /* !_WIN32 */

int
rl_set_keyboard_input_timeout (u)
     int u;
{
  int o;

  o = _keyboard_input_timeout;
  if (u > 0)
    _keyboard_input_timeout = u;
  return (o);
}

#ifndef _WIN32
/* Is there input available to be read on the readline input file
   descriptor?  Only works if the system has select(2) or FIONREAD.
   Uses the value of _keyboard_input_timeout as the timeout; if another
   readline function wants to specify a timeout and not leave it up to
   the user, it should use _rl_input_queued(timeout_value_in_microseconds)
   instead. */
int
_rl_input_available ()
{
#if defined(HAVE_SELECT)
  fd_set readfds, exceptfds;
  struct timeval timeout;
#endif
#if !defined (HAVE_SELECT) && defined(FIONREAD)
  int chars_avail;
#endif
  int tty;

  tty = fileno (rl_instream);

#if defined (HAVE_SELECT)
  FD_ZERO (&readfds);
  FD_ZERO (&exceptfds);
  FD_SET (tty, &readfds);
  FD_SET (tty, &exceptfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = _keyboard_input_timeout;
  return (select (tty + 1, &readfds, (fd_set *)NULL, &exceptfds, &timeout) > 0);
#else

#if defined (FIONREAD)
  if (ioctl (tty, FIONREAD, &chars_avail) == 0)
    return (chars_avail);
#endif

#endif

  return 0;
}
#endif /* !_WIN32 */

int
_rl_input_queued (t)
     int t;
{
  int old_timeout, r;

  old_timeout = rl_set_keyboard_input_timeout (t);
  r = _rl_input_available ();
  rl_set_keyboard_input_timeout (old_timeout);
  return r;
}

void
_rl_insert_typein (c)
     int c;     
{    	
  int key, t, i;
  char *string;

  i = key = 0;
  string = (char *)xmalloc (ibuffer_len + 1);
  string[i++] = (char) c;

  while ((t = rl_get_char (&key)) &&
	 _rl_keymap[key].type == ISFUNC &&
	 _rl_keymap[key].function == rl_insert)
    string[i++] = key;

  if (t)
    _rl_unget_char (key);

  string[i] = '\0';
  rl_insert_text (string);
  free (string);
}

/* Add KEY to the buffer of characters to be read.  Returns 1 if the
   character was stuffed correctly; 0 otherwise. */
int
rl_stuff_char (key)
     int key;
{
  if (ibuffer_space () == 0)
    return 0;

  if (key == EOF)
    {
      key = NEWLINE;
      rl_pending_input = EOF;
      RL_SETSTATE (RL_STATE_INPUTPENDING);
    }
  ibuffer[push_index++] = key;
  if (push_index >= ibuffer_len)
    push_index = 0;

  return 1;
}

/* Make C be the next command to be executed. */
int
rl_execute_next (c)
     int c;
{
  rl_pending_input = c;
  RL_SETSTATE (RL_STATE_INPUTPENDING);
  return 0;
}

/* Clear any pending input pushed with rl_execute_next() */
int
rl_clear_pending_input ()
{
  rl_pending_input = 0;
  RL_UNSETSTATE (RL_STATE_INPUTPENDING);
  return 0;
}

/* **************************************************************** */
/*								    */
/*			     Character Input			    */
/*								    */
/* **************************************************************** */

/* Read a key, including pending input. */
int
rl_read_key ()
{
  int c;

  rl_key_sequence_length++;

  if (rl_pending_input)
    {
      c = rl_pending_input;
      rl_clear_pending_input ();
    }
  else
    {
      /* If input is coming from a macro, then use that. */
      if (c = _rl_next_macro_key ())
	return (c);

      /* If the user has an event function, then call it periodically. */
      if (rl_event_hook)
	{
	  while (rl_event_hook && rl_get_char (&c) == 0)
	    {
	      (*rl_event_hook) ();
	      if (rl_done)		/* XXX - experimental */
		return ('\n');
	      if (rl_gather_tyi () < 0)	/* XXX - EIO */
		{
		  rl_done = 1;
		  return ('\n');
		}
	    }
	}
      else
	{
	  if (rl_get_char (&c) == 0)
	    c = (*rl_getc_function) (rl_instream);
	}
    }

  return (c);
}

#ifndef _WIN32
int
rl_getc (stream)
     FILE *stream;
{
  int result;
  unsigned char c;

  while (1)
    {
      result = read (fileno (stream), &c, sizeof (unsigned char));

      if (result == sizeof (unsigned char))
	return (c);

      /* If zero characters are returned, then the file that we are
	 reading from is empty!  Return EOF in that case. */
      if (result == 0)
	return (EOF);

#if defined (__BEOS__)
      if (errno == EINTR)
	continue;
#endif

#if defined (EWOULDBLOCK)
#  define X_EWOULDBLOCK EWOULDBLOCK
#else
#  define X_EWOULDBLOCK -99
#endif

#if defined (EAGAIN)
#  define X_EAGAIN EAGAIN
#else
#  define X_EAGAIN -99
#endif

      if (errno == X_EWOULDBLOCK || errno == X_EAGAIN)
	{
	  if (sh_unset_nodelay_mode (fileno (stream)) < 0)
	    return (EOF);
	  continue;
	}

#undef X_EWOULDBLOCK
#undef X_EAGAIN

      /* If the error that we received was SIGINT, then try again,
	 this is simply an interrupted system call to read ().
	 Otherwise, some error ocurred, also signifying EOF. */
      if (errno != EINTR)
	return (EOF);
    }
}

#else /* _WIN32 */

#include <windows.h>
#include <ctype.h>
#include <conio.h>
#include <io.h>

#define EXT_PREFIX 0x1f8

#define KEV	   irec.Event.KeyEvent			/* to make life easier  */
#define KST	   irec.Event.KeyEvent.dwControlKeyState

static int pending_key = 0;
static int pending_count = 0;
static int pending_prefix = 0;

extern int _rl_last_c_pos;	/* imported from display.c  */
extern int _rl_last_v_pos;
extern int rl_dispatching;	/* imported from readline.c  */
extern int rl_point;
extern int rl_done;
extern int rl_visible_prompt_length;
extern int _rl_screenwidth;		/* imported from terminal.c  */

extern int haveConsole;		/* imported from rltty.c  */
extern HANDLE hStdout, hStdin;
extern COORD rlScreenOrigin, rlScreenEnd;
extern int rlScreenStart, rlScreenMax;
static void MouseEventProc(MOUSE_EVENT_RECORD kev);

int rl_getc (stream)
     FILE *stream;
{
  if ( pending_count )
    {
      --pending_count;
      if ( pending_prefix && (pending_count & 1) )
        return pending_prefix;
      else
        return pending_key;
    }

  while ( 1 )
    {
      DWORD dummy;

      if (WaitForSingleObject(hStdin, WAIT_FOR_INPUT) != WAIT_OBJECT_0)
        {
          if ( rl_done )
            return( 0 );
          else
            continue;
        }
      if ( haveConsole & FOR_INPUT )
        {
          INPUT_RECORD irec;
          ReadConsoleInput (hStdin, &irec, 1, &dummy);
          switch(irec.EventType)
            {
            case KEY_EVENT:
              if (KEV.bKeyDown &&
                  ((KEV.wVirtualKeyCode < VK_SHIFT) ||
                   (KEV.wVirtualKeyCode > VK_MENU)))
                {
                  pending_count = KEV.wRepeatCount;
                  pending_prefix = 0;
                  pending_key = KEV.uChar.AsciiChar & 0xff;

                  if (KST & ENHANCED_KEY)
                    {
#define CTRL_TO_ASCII(c) ((c) - 'a' + 1)
                      switch (KEV.wVirtualKeyCode)
                        {
                          case VK_HOME:
                            pending_key = CTRL_TO_ASCII ('a');
                            break;
                          case VK_END:
                            pending_key = CTRL_TO_ASCII ('e');
                            break;
                          case VK_LEFT:
                            pending_key = CTRL_TO_ASCII ('b');
                            break;
                          case VK_RIGHT:
                            pending_key = CTRL_TO_ASCII ('f');
                            break;
                          case VK_UP:
                            pending_key = CTRL_TO_ASCII ('p');
                            break;
                          case VK_DOWN:
                            pending_key = CTRL_TO_ASCII ('n');
                            break;
                          case VK_DELETE:
                            pending_key = CTRL_TO_ASCII ('d');
                            break;
                        }
                    }
                  
                  if (KST & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
                    pending_prefix = VK_ESCAPE;

                  if (pending_prefix)
                      pending_count = (pending_count << 1) - 1;
               
                  if (pending_prefix &&
                      (KST & (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED) == (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) &&
                      ((KEV.wVirtualKeyCode >= VK_OEM_1 && KEV.wVirtualKeyCode <= VK_OEM_102) ||
                      (pending_key >= ' ' && pending_key <= '}')))  {
                      pending_prefix = 0;
                  }

                  /* Ascii direct */
                  if (pending_key)
                      pending_count--;

                  if (pending_prefix)
                    return pending_prefix;
                  return pending_key;
                }
              break;
            case MOUSE_EVENT:
              if ( (haveConsole & FOR_OUTPUT) && !rl_dispatching )
                MouseEventProc(irec.Event.MouseEvent);
            default:
              break;
            }
        }
      else
        {
          int key;
          ReadFile(hStdin, &key, 1, &dummy, NULL);
          return key;
        }
    }
}

void MouseEventProc(MOUSE_EVENT_RECORD mev)
{
  static DWORD lastButtonState, cstat_flags;
  static COORD lastButtonPos, src_down_pos;

#define RLPOS_CHANGED	1
#define SELECT_START	2
  
  switch (mev.dwEventFlags )
    {
    case 0 :			/* change in button state  */

      /* Cursor setting: 
	 LEFT_BUTTON_PRESSED sets cursor anywhere on the screen,
	 thereafter, any change in button state will clipp the cursor
	 position to the readline range if there has been no cursor
	 movement. Otherwhise the cursor is reset to its old position.
      */
      if (mev.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
        {
          if (lastButtonState == 0)
            {
              src_down_pos = mev.dwMousePosition;
              cstat_flags |= RLPOS_CHANGED | SELECT_START;
              SetConsoleCursorPosition(hStdout, mev.dwMousePosition);
            }
        }
      else
        {
          if (cstat_flags & RLPOS_CHANGED)
            {
              if ( (mev.dwMousePosition.X == src_down_pos.X)
		   && (mev.dwMousePosition.Y == src_down_pos.Y) )
                {
                  int linear_pos = (int)mev.dwMousePosition.Y * _rl_screenwidth
		    + (int)mev.dwMousePosition.X;
                  if (linear_pos < rlScreenStart + rl_visible_prompt_length)
                    {
                      linear_pos = rlScreenStart + rl_visible_prompt_length;
                      mev.dwMousePosition.X = rlScreenOrigin.X + rl_visible_prompt_length;
                      mev.dwMousePosition.Y = rlScreenOrigin.Y;
                    }
                  if (linear_pos > rlScreenMax)
                    {
                      linear_pos = rlScreenMax;
                      mev.dwMousePosition = rlScreenEnd;
                    }
                  rl_point = linear_pos - rlScreenStart - rl_visible_prompt_length;
                  _rl_last_c_pos = mev.dwMousePosition.X - rlScreenOrigin.X;
                  _rl_last_v_pos = mev.dwMousePosition.Y - rlScreenOrigin.Y;
                }
              else
                {
                  mev.dwMousePosition.X = rlScreenOrigin.X + _rl_last_c_pos;
                  mev.dwMousePosition.Y = rlScreenOrigin.Y + _rl_last_v_pos;
                }
              SetConsoleCursorPosition(hStdout, mev.dwMousePosition);
              cstat_flags &= !RLPOS_CHANGED;
            }
        }
      lastButtonState = mev.dwButtonState;
      lastButtonPos = mev.dwMousePosition;
      break;
    case MOUSE_MOVED:		/* the most frequent event */
    default:      
      break;
    }
}

int _rl_input_available ()
{
  if (isatty (fileno (rl_instream)))
    return (kbhit());
  return 0;
}

static int rl_gather_tyi ()
{
  int count = 0;
  while (isatty (fileno (rl_instream)) && kbhit () && ibuffer_space ())
    {
      rl_stuff_char ((*rl_getc_function) (rl_instream));
      count++;
    }
  return count;
}
#endif /* _WIN32 */

#if defined (HANDLE_MULTIBYTE)
/* read multibyte char */
int
_rl_read_mbchar (mbchar, size)
     char *mbchar;
     int size;
{
  int mb_len = 0;
  size_t mbchar_bytes_length;
  wchar_t wc;
  mbstate_t ps, ps_back;

  memset(&ps, 0, sizeof (mbstate_t));
  memset(&ps_back, 0, sizeof (mbstate_t));
  
  while (mb_len < size)
    {
      RL_SETSTATE(RL_STATE_MOREINPUT);
      mbchar[mb_len++] = rl_read_key ();
      RL_UNSETSTATE(RL_STATE_MOREINPUT);

      mbchar_bytes_length = mbrtowc (&wc, mbchar, mb_len, &ps);
      if (mbchar_bytes_length == (size_t)(-1))
	break;		/* invalid byte sequence for the current locale */
      else if (mbchar_bytes_length == (size_t)(-2))
	{
	  /* shorted bytes */
	  ps = ps_back;
	  continue;
	} 
      else if (mbchar_bytes_length > (size_t)(0))
	break;
    }

  return mb_len;
}

/* Read a multibyte-character string whose first character is FIRST into
   the buffer MB of length MBLEN.  Returns the last character read, which
   may be FIRST.  Used by the search functions, among others.  Very similar
   to _rl_read_mbchar. */
int
_rl_read_mbstring (first, mb, mblen)
     int first;
     char *mb;
     int mblen;
{
  int i, c;
  mbstate_t ps;

  c = first;
  memset (mb, 0, mblen);
  for (i = 0; i < mblen; i++)
    {
      mb[i] = (char)c;
      memset (&ps, 0, sizeof (mbstate_t));
      if (_rl_get_char_len (mb, &ps) == -2)
	{
	  /* Read more for multibyte character */
	  RL_SETSTATE (RL_STATE_MOREINPUT);
	  c = rl_read_key ();
	  RL_UNSETSTATE (RL_STATE_MOREINPUT);
	}
      else
	break;
    }
  return c;
}
#endif /* HANDLE_MULTIBYTE */
