/*  =========================================================================
    czmq_prelude.h - CZMQ environment

    -------------------------------------------------------------------------
    Copyright (c) 1991-2011 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of CZMQ, the high-level C binding for 0MQ:
    http://czmq.zeromq.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at
    your option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#ifndef __CZMQ_PRELUDE_H_INCLUDED__
#define __CZMQ_PRELUDE_H_INCLUDED__

//- Always include ZeroMQ header file ---------------------------------------

#include "zmq.h"

//  Older libzmq APIs will be missing some aspects of libzmq/3.0

#ifndef ZMQ_ROUTER
#   define ZMQ_ROUTER       ZMQ_XREP
#endif
#ifndef ZMQ_DEALER
#   define ZMQ_DEALER       ZMQ_XREQ
#endif
#ifndef ZMQ_DONTWAIT
#   define ZMQ_DONTWAIT     ZMQ_NOBLOCK
#endif
#ifndef ZMQ_XSUB
#   error "please upgrade to latest stable libzmq from http://zeromq.org"
#endif
#if ZMQ_VERSION_MAJOR == 2
#   if  ZMQ_VERSION_MINOR == 0 \
    || (ZMQ_VERSION_MINOR == 1 && ZMQ_VERSION_PATCH < 7)
#       error "CZMQ requires at least libzmq/2.1.7 stable"
#   endif
#   define zmq_sendmsg      zmq_send
#   define zmq_recvmsg      zmq_recv
#   define ZMQ_POLL_MSEC    1000        //  zmq_poll is usec
#elif ZMQ_VERSION_MAJOR == 3 || ZMQ_VERSION_MAJOR == 4
#   define ZMQ_POLL_MSEC    1           //  zmq_poll is msec
#endif

//- Establish the compiler and computer system ------------------------------
/*
 *  Defines zero or more of these symbols, for use in any non-portable
 *  code:
 *
 *  __WINDOWS__         Microsoft C/C++ with Windows calls
 *  __MSDOS__           System is MS-DOS (set if __WINDOWS__ set)
 *  __VMS__             System is VAX/VMS or Alpha/OpenVMS
 *  __UNIX__            System is UNIX
 *  __OS2__             System is OS/2
 *
 *  __IS_32BIT__        OS/compiler is 32 bits
 *  __IS_64BIT__        OS/compiler is 64 bits
 *
 *  When __UNIX__ is defined, we also define exactly one of these:
 *
 *  __UTYPE_AUX         Apple AUX
 *  __UTYPE_BEOS        BeOS
 *  __UTYPE_BSDOS       BSD/OS
 *  __UTYPE_DECALPHA    Digital UNIX (Alpha)
 *  __UTYPE_IBMAIX      IBM RS/6000 AIX
 *  __UTYPE_FREEBSD     FreeBSD
 *  __UTYPE_HPUX        HP/UX
 *  __UTYPE_ANDROID     Android
 *  __UTYPE_LINUX       Linux
 *  __UTYPE_MIPS        MIPS (BSD 4.3/System V mixture)
 *  __UTYPE_NETBSD      NetBSD
 *  __UTYPE_NEXT        NeXT
 *  __UTYPE_OPENBSD     OpenBSD
 *  __UTYPE_OSX         Apple Macintosh OS X
 *  __UTYPE_QNX         QNX
 *  __UTYPE_IRIX        Silicon Graphics IRIX
 *  __UTYPE_SINIX       SINIX-N (Siemens-Nixdorf Unix)
 *  __UTYPE_SUNOS       SunOS
 *  __UTYPE_SUNSOLARIS  Sun Solaris
 *  __UTYPE_UNIXWARE    SCO UnixWare
 *                      ... these are the ones I know about so far.
 *  __UTYPE_GENERIC     Any other UNIX
 *
 *  When __VMS__ is defined, we may define one or more of these:
 *
 *  __VMS_XOPEN         Supports XOPEN functions
 */

#if (defined (__64BIT__) || defined (__x86_64__))
#    define __IS_64BIT__                //  May have 64-bit OS/compiler
#else
#    define __IS_32BIT__                //  Else assume 32-bit OS/compiler
#endif

#if (defined WIN32 || defined _WIN32)
#   undef __WINDOWS__
#   define __WINDOWS__
#   undef __MSDOS__
#   define __MSDOS__
#endif

#if (defined WINDOWS || defined _WINDOWS || defined __WINDOWS__)
#   undef __WINDOWS__
#   define __WINDOWS__
#   undef __MSDOS__
#   define __MSDOS__
#   if _MSC_VER == 1500
#       ifndef _CRT_SECURE_NO_DEPRECATE
#           define _CRT_SECURE_NO_DEPRECATE   1
#       endif
#       pragma warning(disable: 4996)
#   endif
#endif

//  MSDOS               Microsoft C
//  _MSC_VER            Microsoft C
#if (defined (MSDOS) || defined (_MSC_VER))
#   undef __MSDOS__
#   define __MSDOS__
#   if (defined (_DEBUG) && !defined (DEBUG))
#       define DEBUG
#   endif
#endif

#if (defined (__EMX__) && defined (__i386__))
#   undef __OS2__
#   define __OS2__
#endif

//  VMS                 VAX C (VAX/VMS)
//  __VMS               Dec C (Alpha/OpenVMS)
//  __vax__             gcc
#if (defined (VMS) || defined (__VMS) || defined (__vax__))
#   undef __VMS__
#   define __VMS__
#   if (__VMS_VER >= 70000000)
#       define __VMS_XOPEN
#   endif
#endif

//  Try to define a __UTYPE_xxx symbol...
//  unix                SunOS at least
//  __unix__            gcc
//  _POSIX_SOURCE is various UNIX systems, maybe also VAX/VMS
#if (defined (unix) || defined (__unix__) || defined (_POSIX_SOURCE))
#   if (!defined (__VMS__))
#       undef __UNIX__
#       define __UNIX__
#       if (defined (__alpha))          //  Digital UNIX is 64-bit
#           undef  __IS_32BIT__
#           define __IS_64BIT__
#           define __UTYPE_DECALPHA
#       endif
#   endif
#endif

#if (defined (_AUX))
#   define __UTYPE_AUX
#   define __UNIX__
#elif (defined (__BEOS__))
#   define __UTYPE_BEOS
#   define __UNIX__
#elif (defined (__hpux))
#   define __UTYPE_HPUX
#   define __UNIX__
#   define _INCLUDE_HPUX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _INCLUDE_POSIX_SOURCE
#elif (defined (_AIX) || defined (AIX))
#   define __UTYPE_IBMAIX
#   define __UNIX__
#elif (defined (BSD) || defined (bsd))
#   define __UTYPE_BSDOS
#   define __UNIX__
#elif (defined (APPLE) || defined (__APPLE__))
#   define __UTYPE_GENERIC
#   define __UNIX__
#elif (defined (__ANDROID__))
#   define __UTYPE_ANDROID
#   define __UNIX__
#elif (defined (LINUX) || defined (linux))
#   define __UTYPE_LINUX
#   define __UNIX__
#   ifndef __NO_CTYPE
#   define __NO_CTYPE                   //  Suppress warnings on tolower()
#   endif
#elif (defined (Mips))
#   define __UTYPE_MIPS
#   define __UNIX__
#elif (defined (FreeBSD) || defined (__FreeBSD__))
#   define __UTYPE_FREEBSD
#   define __UNIX__
#elif (defined (NetBSD) || defined (__NetBSD__))
#   define __UTYPE_NETBSD
#   define __UNIX__
#elif (defined (OpenBSD) || defined (__OpenBSD__))
#   define __UTYPE_OPENBSD
#   define __UNIX__
#elif (defined (NeXT))
#   define __UTYPE_NEXT
#   define __UNIX__
#elif (defined (__QNX__))
#   define __UTYPE_QNX
#   define __UNIX__
#elif (defined (sgi))
#   define __UTYPE_IRIX
#   define __UNIX__
#elif (defined (sinix))
#   define __UTYPE_SINIX
#   define __UNIX__
#elif (defined (SOLARIS) || defined (__SRV4))
#   define __UTYPE_SUNSOLARIS
#   define __UNIX__
#elif (defined (SUNOS) || defined (SUN) || defined (sun))
#   define __UTYPE_SUNOS
#   define __UNIX__
#elif (defined (__USLC__) || defined (UnixWare))
#   define __UTYPE_UNIXWARE
#   define __UNIX__
#elif (defined (__CYGWIN__))
#   define __UTYPE_CYGWIN
#   define __UNIX__
#elif (defined (__UNIX__))
#   define __UTYPE_GENERIC
#endif

//- Standard ANSI include files ---------------------------------------------

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <signal.h>
#include <setjmp.h>
#include <assert.h>

//- System-specific include files -------------------------------------------

#if (defined (__MSDOS__))
#   if (defined (__WINDOWS__))
#       if (!defined (FD_SETSIZE))
#           define FD_SETSIZE 1024      //  Max. filehandles/sockets
#       endif
#       include <direct.h>
#       include <windows.h>
#       include <winsock.h>
#       include <process.h>
#   endif
#   include <malloc.h>
#   include <dos.h>
#   include <io.h>
#   include <fcntl.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/utime.h>
#   include <share.h>
#   if _MSC_VER == 1500
#       ifndef _CRT_SECURE_NO_DEPRECATE
#           define _CRT_SECURE_NO_DEPRECATE   1
#       endif
#       pragma warning(disable: 4996)
#   endif
#endif

#if (defined (__UNIX__))
#   include <fcntl.h>
#   include <netdb.h>
#   include <unistd.h>
#   include <pthread.h>
#   include <dirent.h>
#   include <pwd.h>
#   include <grp.h>
#   include <utime.h>
#   include <syslog.h>
#   include <inttypes.h>
#   include <sys/types.h>
#   include <sys/param.h>
#   include <sys/socket.h>
#   include <sys/time.h>
#   include <sys/stat.h>
#   include <sys/ioctl.h>
#   include <sys/file.h>
#   include <sys/wait.h>
#   include <netinet/in.h>              //  Must come before arpa/inet.h
#   if (!defined (__UTYPE_ANDROID))
#       include <ifaddrs.h>
#   endif
#   if (!defined (__UTYPE_BEOS))
#       include <arpa/inet.h>
#       if (!defined (TCP_NODELAY))
#           include <netinet/tcp.h>
#       endif
#   endif
#   if (defined (__UTYPE_IBMAIX) || defined(__UTYPE_QNX))
#       include <sys/select.h>
#   endif
#   if (defined (__UTYPE_BEOS))
#       include <NetKit.h>
#   endif
#   if ((defined (_XOPEN_REALTIME) && (_XOPEN_REALTIME >= 1)) || \
        (defined (_POSIX_VERSION)  && (_POSIX_VERSION  >= 199309L)))
#       include <sched.h>
#   endif
#   if (defined (__UTYPE_OSX))
#       include <crt_externs.h>         /* For _NSGetEnviron()               */
#   endif
#endif

#if (defined (__VMS__))
#   if (!defined (vaxc))
#       include <fcntl.h>               //  Not provided by Vax C
#   endif
#   include <netdb.h>
#   include <unistd.h>
#   include <pthread.h>
#   include <unixio.h>
#   include <unixlib.h>
#   include <types.h>
#   include <file.h>
#   include <socket.h>
#   include <dirent.h>
#   include <time.h>
#   include <pwd.h>
#   include <stat.h>
#   include <in.h>
#   include <inet.h>
#endif

#if (defined (__OS2__))
#   include <sys/types.h>               //  Required near top
#   include <fcntl.h>
#   include <malloc.h>
#   include <netdb.h>
#   include <unistd.h>
#   include <pthread.h>
#   include <dirent.h>
#   include <pwd.h>
#   include <grp.h>
#   include <io.h>
#   include <process.h>
#   include <sys/param.h>
#   include <sys/socket.h>
#   include <sys/select.h>
#   include <sys/time.h>
#   include <sys/stat.h>
#   include <sys/ioctl.h>
#   include <sys/file.h>
#   include <sys/wait.h>
#   include <netinet/in.h>              //  Must come before arpa/inet.h
#   include <arpa/inet.h>
#   include <utime.h>
#   if (!defined (TCP_NODELAY))
#       include <netinet/tcp.h>
#   endif
#endif

//- Check compiler data type sizes ------------------------------------------

#if (UCHAR_MAX != 0xFF)
#   error "Cannot compile: must change definition of 'byte'."
#endif
#if (USHRT_MAX != 0xFFFFU)
#    error "Cannot compile: must change definition of 'dbyte'."
#endif
#if (UINT_MAX != 0xFFFFFFFFU)
#    error "Cannot compile: must change definition of 'qbyte'."
#endif

//- Data types --------------------------------------------------------------

typedef          int    Bool;           //  Boolean TRUE/FALSE value
typedef unsigned char   byte;           //  Single unsigned byte = 8 bits
typedef unsigned short  dbyte;          //  Double byte = 16 bits
typedef unsigned int    qbyte;          //  Quad byte = 32 bits

//- Inevitable macros -------------------------------------------------------

#define streq(s1,s2)    (!strcmp ((s1), (s2)))
#define strneq(s1,s2)   (strcmp ((s1), (s2)))
#define tblsize(x)      (sizeof (x) / sizeof ((x) [0]))
#define tbllast(x)      (x [tblsize (x) - 1])
//  Provide random number from 0..(num-1)
#if (defined (__WINDOWS__))
#   define randof(num)  (int) ((float) (num) * rand () / (RAND_MAX + 1.0))
#else
#   define randof(num)  (int) ((float) (num) * random () / (RAND_MAX + 1.0))
#endif
#if (!defined (TRUE))
#    define TRUE        1               //  ANSI standard
#    define FALSE       0
#endif

//- A number of POSIX and C99 keywords and data types -----------------------

#if (defined (__WINDOWS__))
#   define inline __inline
#   define strtoull _strtoui64
#   define srandom      srand
#   define TIMEZONE     _timezone
#   define snprintf     _snprintf
#   define vsnprintf    _vsnprintf
    typedef unsigned long ulong;
    typedef unsigned int  uint;
    typedef __int32 int32_t;
    typedef __int64 int64_t;
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int64 uint64_t;
#elif (defined (__APPLE__))
    typedef unsigned long ulong;
    typedef unsigned int  uint;
#endif

//- Error reporting ---------------------------------------------------------
// If the compiler is GCC or supports C99, include enclosing function
// in czmq assertions
#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#   define CZMQ_ASSERT_SANE_FUNCTION    __func__
#elif defined (__GNUC__) && (__GNUC__ >= 2)
#   define CZMQ_ASSERT_SANE_FUNCTION    __FUNCTION__
#else
#   define CZMQ_ASSERT_SANE_FUNCTION    "<unknown>"
#endif

//  Replacement for malloc() which asserts if we run out of heap, and
//  which zeroes the allocated block.
static inline void *
    safe_malloc (
    size_t size,
    char *file,
    unsigned line,
    const char *func)
{
    void
        *mem;

    mem = calloc (1, size);
    if (mem == NULL) {
        fprintf (stderr, "FATAL ERROR at %s:%u, in %s\n", file, line, func);
        fprintf (stderr, "OUT OF MEMORY (malloc returned NULL)\n");
        fflush (stderr);
        abort ();
    }
    return mem;
}

//  Define _ZMALLOC_DEBUG if you need to trace memory leaks using e.g. mtrace,
//  otherwise all allocations will claim to come from zfl_prelude.h.  For best
//  results, compile all classes so you see dangling object allocations.
//
#ifdef _ZMALLOC_DEBUG
#   define zmalloc(size) calloc(1,(size))
#else
#   define zmalloc(size) safe_malloc((size), __FILE__, __LINE__, CZMQ_ASSERT_SANE_FUNCTION)
#endif

//- DLL exports -------------------------------------------------------------

#if (defined (__WINDOWS__))
#   if defined DLL_EXPORT
#       define CZMQ_EXPORT __declspec(dllexport)
#   else
#       define CZMQ_EXPORT __declspec(dllimport)
#   endif
#else
#   define CZMQ_EXPORT
#endif

#endif
