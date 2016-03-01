/*
 *  getopt.h - cpp wrapper for my_getopt to make it look like getopt.
 *  Copyright 1997, 2000, 2001, 2002, Benjamin Sittler
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifndef MY_WRAPPER_GETOPT_H_INCLUDED
#define MY_WRAPPER_GETOPT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "mygetopt.h"

#undef getopt
#define getopt my_getopt
#undef getopt_long
#define getopt_long my_getopt_long
#undef getopt_long_only
#define getopt_long_only my_getopt_long_only
#undef _getopt_internal
#define _getopt_internal _my_getopt_internal
#undef opterr
#define opterr my_opterr
#undef optind
#define optind my_optind
#undef optopt
#define optopt my_optopt
#undef optarg
#define optarg my_optarg

#ifdef __cplusplus
}
#endif

#endif /* MY_WRAPPER_GETOPT_H_INCLUDED */
