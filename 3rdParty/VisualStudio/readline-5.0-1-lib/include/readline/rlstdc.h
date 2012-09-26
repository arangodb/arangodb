/* stdc.h -- macros to make source compile on both ANSI C and K&R C
   compilers. */

/* Copyright (C) 1993 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#if !defined (_RL_STDC_H_)
#define _RL_STDC_H_

/* Adapted from BSD /usr/include/sys/cdefs.h. */

/* A function can be defined using prototypes and compile on both ANSI C
   and traditional C compilers with something like this:
	extern char *func PARAMS((char *, char *, int)); */

#if !defined (PARAMS)
#  if defined (__STDC__) || defined (__GNUC__) || defined (__cplusplus)
#    define PARAMS(protos) protos
#  else
#    define PARAMS(protos) ()
#  endif
#endif

#ifndef __attribute__
#  if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#    define __attribute__(x)
#  endif
#endif

#ifndef __GNUC__
# define __DLL_IMPORT__  __declspec(dllimport)
# define __DLL_EXPORT__  __declspec(dllexport)
#else
# define __DLL_IMPORT__  __attribute__((dllimport)) extern
# define __DLL_EXPORT__  __attribute__((dllexport)) extern
#endif 

#if (defined __WIN32__) || (defined _WIN32)
# ifdef BUILD_READLINE_DLL
#  define READLINE_DLL_IMPEXP     __DLL_EXPORT__
# elif defined(READLINE_STATIC)
#  define READLINE_DLL_IMPEXP      
# elif defined (USE_READLINE_DLL)
#  define READLINE_DLL_IMPEXP     __DLL_IMPORT__
# elif defined (USE_READLINE_STATIC)
#  define READLINE_DLL_IMPEXP      
# else /* assume USE_READLINE_DLL */
#  define READLINE_DLL_IMPEXP     __DLL_IMPORT__
# endif
#else /* __WIN32__ */
# define READLINE_DLL_IMPEXP  
#endif

#endif /* !_RL_STDC_H_ */
