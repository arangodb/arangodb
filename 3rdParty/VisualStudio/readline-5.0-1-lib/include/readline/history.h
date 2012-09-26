/* history.h -- the names of functions that you can call in history. */
/* Copyright (C) 1989-2003 Free Software Foundation, Inc.

   This file contains the GNU History Library (the Library), a set of
   routines for managing the text of previously typed lines.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#ifndef _HISTORY_H_
#define _HISTORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>		/* XXX - for history timestamp code */

#if defined READLINE_LIBRARY
#  include "rlstdc.h"
#  include "rltypedefs.h"
#else
#  include <readline/rlstdc.h>
#  include <readline/rltypedefs.h>
#endif

#ifdef __STDC__
typedef void *histdata_t;
#else
typedef char *histdata_t;
#endif

/* The structure used to store a history entry. */
typedef struct _hist_entry {
  char *line;
  char *timestamp;		/* char * rather than time_t for read/write */
  histdata_t data;
} HIST_ENTRY;

/* Size of the history-library-managed space in history entry HS. */
#define HISTENT_BYTES(hs)	(strlen ((hs)->line) + strlen ((hs)->timestamp))

/* A structure used to pass the current state of the history stuff around. */
typedef struct _hist_state {
  HIST_ENTRY **entries;		/* Pointer to the entries themselves. */
  int offset;			/* The location pointer within this array. */
  int length;			/* Number of elements within this array. */
  int size;			/* Number of slots allocated to this array. */
  int flags;
} HISTORY_STATE;

/* Flag values for the `flags' member of HISTORY_STATE. */
#define HS_STIFLED	0x01

/* Initialization and state management. */

/* Begin a session in which the history functions might be used.  This
   just initializes the interactive variables. */
READLINE_DLL_IMPEXP void using_history PARAMS((void));

/* Return the current HISTORY_STATE of the history. */
READLINE_DLL_IMPEXP HISTORY_STATE *history_get_history_state PARAMS((void));

/* Set the state of the current history array to STATE. */
READLINE_DLL_IMPEXP void history_set_history_state PARAMS((HISTORY_STATE *));

/* Manage the history list. */

/* Place STRING at the end of the history list.
   The associated data field (if any) is set to NULL. */
READLINE_DLL_IMPEXP void add_history PARAMS((const char *));

/* Change the timestamp associated with the most recent history entry to
   STRING. */
READLINE_DLL_IMPEXP void add_history_time PARAMS((const char *));

/* A reasonably useless function, only here for completeness.  WHICH
   is the magic number that tells us which element to delete.  The
   elements are numbered from 0. */
READLINE_DLL_IMPEXP HIST_ENTRY *remove_history PARAMS((int));

/* Free the history entry H and return any application-specific data
   associated with it. */
READLINE_DLL_IMPEXP histdata_t free_history_entry PARAMS((HIST_ENTRY *));

/* Make the history entry at WHICH have LINE and DATA.  This returns
   the old entry so you can dispose of the data.  In the case of an
   invalid WHICH, a NULL pointer is returned. */
READLINE_DLL_IMPEXP HIST_ENTRY *replace_history_entry PARAMS((int, const char *, histdata_t));

/* Clear the history list and start over. */
READLINE_DLL_IMPEXP void clear_history PARAMS((void));

/* Stifle the history list, remembering only MAX number of entries. */
READLINE_DLL_IMPEXP void stifle_history PARAMS((int));

/* Stop stifling the history.  This returns the previous amount the
   history was stifled by.  The value is positive if the history was
   stifled, negative if it wasn't. */
READLINE_DLL_IMPEXP int unstifle_history PARAMS((void));

/* Return 1 if the history is stifled, 0 if it is not. */
READLINE_DLL_IMPEXP int history_is_stifled PARAMS((void));

/* Information about the history list. */

/* Return a NULL terminated array of HIST_ENTRY which is the current input
   history.  Element 0 of this list is the beginning of time.  If there
   is no history, return NULL. */
READLINE_DLL_IMPEXP HIST_ENTRY **history_list PARAMS((void));

/* Returns the number which says what history element we are now
   looking at.  */
READLINE_DLL_IMPEXP int where_history PARAMS((void));
  
/* Return the history entry at the current position, as determined by
   history_offset.  If there is no entry there, return a NULL pointer. */
READLINE_DLL_IMPEXP HIST_ENTRY *current_history PARAMS((void));

/* Return the history entry which is logically at OFFSET in the history
   array.  OFFSET is relative to history_base. */
READLINE_DLL_IMPEXP HIST_ENTRY *history_get PARAMS((int));

/* Return the timestamp associated with the HIST_ENTRY * passed as an
   argument */
READLINE_DLL_IMPEXP time_t history_get_time PARAMS((HIST_ENTRY *));

/* Return the number of bytes that the primary history entries are using.
   This just adds up the lengths of the_history->lines. */
READLINE_DLL_IMPEXP int history_total_bytes PARAMS((void));

/* Moving around the history list. */

/* Set the position in the history list to POS. */
READLINE_DLL_IMPEXP int history_set_pos PARAMS((int));

/* Back up history_offset to the previous history entry, and return
   a pointer to that entry.  If there is no previous entry, return
   a NULL pointer. */
READLINE_DLL_IMPEXP HIST_ENTRY *previous_history PARAMS((void));

/* Move history_offset forward to the next item in the input_history,
   and return the a pointer to that entry.  If there is no next entry,
   return a NULL pointer. */
READLINE_DLL_IMPEXP HIST_ENTRY *next_history PARAMS((void));

/* Searching the history list. */

/* Search the history for STRING, starting at history_offset.
   If DIRECTION < 0, then the search is through previous entries,
   else through subsequent.  If the string is found, then
   current_history () is the history entry, and the value of this function
   is the offset in the line of that history entry that the string was
   found in.  Otherwise, nothing is changed, and a -1 is returned. */
READLINE_DLL_IMPEXP int history_search PARAMS((const char *, int));

/* Search the history for STRING, starting at history_offset.
   The search is anchored: matching lines must begin with string.
   DIRECTION is as in history_search(). */
READLINE_DLL_IMPEXP int history_search_prefix PARAMS((const char *, int));

/* Search for STRING in the history list, starting at POS, an
   absolute index into the list.  DIR, if negative, says to search
   backwards from POS, else forwards.
   Returns the absolute index of the history element where STRING
   was found, or -1 otherwise. */
READLINE_DLL_IMPEXP int history_search_pos PARAMS((const char *, int, int));

/* Managing the history file. */

/* Add the contents of FILENAME to the history list, a line at a time.
   If FILENAME is NULL, then read from ~/.history.  Returns 0 if
   successful, or errno if not. */
READLINE_DLL_IMPEXP int read_history PARAMS((const char *));

/* Read a range of lines from FILENAME, adding them to the history list.
   Start reading at the FROM'th line and end at the TO'th.  If FROM
   is zero, start at the beginning.  If TO is less than FROM, read
   until the end of the file.  If FILENAME is NULL, then read from
   ~/.history.  Returns 0 if successful, or errno if not. */
READLINE_DLL_IMPEXP int read_history_range PARAMS((const char *, int, int));

/* Write the current history to FILENAME.  If FILENAME is NULL,
   then write the history list to ~/.history.  Values returned
   are as in read_history ().  */
READLINE_DLL_IMPEXP int write_history PARAMS((const char *));

/* Append NELEMENT entries to FILENAME.  The entries appended are from
   the end of the list minus NELEMENTs up to the end of the list. */
READLINE_DLL_IMPEXP int append_history PARAMS((int, const char *));

/* Truncate the history file, leaving only the last NLINES lines. */
READLINE_DLL_IMPEXP int history_truncate_file PARAMS((const char *, int));

/* History expansion. */

/* Expand the string STRING, placing the result into OUTPUT, a pointer
   to a string.  Returns:

   0) If no expansions took place (or, if the only change in
      the text was the de-slashifying of the history expansion
      character)
   1) If expansions did take place
  -1) If there was an error in expansion.
   2) If the returned line should just be printed.

  If an error ocurred in expansion, then OUTPUT contains a descriptive
  error message. */
READLINE_DLL_IMPEXP int history_expand PARAMS((char *, char **));

/* Extract a string segment consisting of the FIRST through LAST
   arguments present in STRING.  Arguments are broken up as in
   the shell. */
READLINE_DLL_IMPEXP char *history_arg_extract PARAMS((int, int, const char *));

/* Return the text of the history event beginning at the current
   offset into STRING.  Pass STRING with *INDEX equal to the
   history_expansion_char that begins this specification.
   DELIMITING_QUOTE is a character that is allowed to end the string
   specification for what to search for in addition to the normal
   characters `:', ` ', `\t', `\n', and sometimes `?'. */
READLINE_DLL_IMPEXP char *get_history_event PARAMS((const char *, int *, int));

/* Return an array of tokens, much as the shell might.  The tokens are
   parsed out of STRING. */
READLINE_DLL_IMPEXP char **history_tokenize PARAMS((const char *));

/* Exported history variables. */
READLINE_DLL_IMPEXP int history_base;
READLINE_DLL_IMPEXP int history_length;
READLINE_DLL_IMPEXP int history_max_entries;
READLINE_DLL_IMPEXP char history_expansion_char;
READLINE_DLL_IMPEXP char history_subst_char;
READLINE_DLL_IMPEXP char *history_word_delimiters;
READLINE_DLL_IMPEXP char history_comment_char;
READLINE_DLL_IMPEXP char *history_no_expand_chars;
READLINE_DLL_IMPEXP char *history_search_delimiter_chars;
READLINE_DLL_IMPEXP int history_quotes_inhibit_expansion;

READLINE_DLL_IMPEXP int history_write_timestamps;

/* Backwards compatibility */
READLINE_DLL_IMPEXP int max_input_history;

/* If set, this function is called to decide whether or not a particular
   history expansion should be treated as a special case for the calling
   application and not expanded. */
READLINE_DLL_IMPEXP rl_linebuf_func_t *history_inhibit_expansion_function;

#ifdef __cplusplus
}
#endif

#endif /* !_HISTORY_H_ */
