/*
    Copyright 2007 Rene Rivera
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef BJAM_OUTPUT_H
#define BJAM_OUTPUT_H

#include "object.h"
#include "timestamp.h"

#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_TIMEOUT 2

void out_action(
    char const * const action,
    char const * const target,
    char const * const command,
    char const * const out_data,
    char const * const err_data,
    int const exit_reason
);

void out_flush();
void err_flush();
void out_puts(char const * const s);
void err_puts(char const * const s);
void out_putc(const char c);
void err_putc(const char c);
void out_data(char const * const s);
void err_data(char const * const s);
void out_printf(char const * const f, ...);
void err_printf(char const * const f, ...);

OBJECT * outf_int( int const value );
OBJECT * outf_double( double const value );
OBJECT * outf_time( timestamp const * const value );

#endif
