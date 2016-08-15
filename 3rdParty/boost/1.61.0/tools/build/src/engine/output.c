/*
    Copyright 2007 Rene Rivera
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
*/

#include "jam.h"
#include "output.h"

#include <stdio.h>
#include <stdarg.h>


#define bjam_out (stdout)
#define bjam_err (stderr)

static void out_( char const * data, FILE * const io )
{
    while ( *data )
    {
        size_t const len = strcspn( data, "\r" );
        data += fwrite( data, 1, len, io );
        if ( *data == '\r' ) ++data;
    }
}


void out_flush()
{
    fflush( bjam_out );
    if ( globs.out ) fflush( globs.out );
}
void err_flush()
{
    fflush( bjam_err );
    if ( globs.out ) fflush( globs.out );
}
void out_puts(char const * const s)
{
    fputs( s, bjam_out );
    if ( globs.out ) fputs( s, globs.out );
}
void err_puts(char const * const s)
{
    fputs( s, bjam_err );
    if ( globs.out ) fputs( s, globs.out );
}
void out_putc(const char c)
{
    fputc( c, bjam_out );
    if ( globs.out ) fputc( c, globs.out );
}
void err_putc(const char c)
{
    fputc( c, bjam_err );
    if ( globs.out ) fputc( c, globs.out );
}
void out_data(char const * const s)
{
    out_( s, bjam_out );
    if ( globs.out ) out_( s, globs.out );
}
void err_data(char const * const s)
{
    out_( s, bjam_err );
    if ( globs.out ) out_( s, globs.out );
}
void out_printf(char const * const f, ...)
{
    {
        va_list args;
        va_start( args, f );
        vfprintf( bjam_out, f, args );
        va_end( args );
    }
    if ( globs.out )
    {
        va_list args;
        va_start( args, f );
        vfprintf( globs.out, f, args );
        va_end( args );
    }
}
void err_printf(char const * const f, ...)
{
    {
        va_list args;
        va_start( args, f );
        vfprintf( bjam_err, f, args );
        va_end( args );
    }
    if ( globs.out )
    {
        va_list args;
        va_start( args, f );
        vfprintf( globs.out, f, args );
        va_end( args );
    }
}


void out_action
(
    char const * const action,
    char const * const target,
    char const * const command,
    char const * const out_d,
    char const * const err_d,
    int const exit_reason
)
{
    /* Print out the action + target line, if the action is quiet the action
     * should be null.
     */
    if ( action )
        out_printf( "%s %s\n", action, target );

    /* Print out the command executed if given -d+2. */
    if ( DEBUG_EXEC )
    {
        out_puts( command );
        out_putc( '\n' );
    }

    /* If the process expired, make user aware with an explicit message, but do
     * this only for non-quiet actions.
     */
    if ( exit_reason == EXIT_TIMEOUT && action )
        out_printf( "%ld second time limit exceeded\n", globs.timeout );

    /* Print out the command output, if requested, or if the program failed, but
     * only output for non-quiet actions.
     */
    if ( action || exit_reason != EXIT_OK )
    {
        if ( out_d &&
           ( ( globs.pipe_action & 1 /* STDOUT_FILENO */ ) ||
             ( globs.pipe_action == 0 ) ) )
            out_data( out_d );
        if ( err_d && ( globs.pipe_action & 2 /* STDERR_FILENO */ ) )
            err_data( err_d );
    }

    out_flush();
    err_flush();
}


OBJECT * outf_int( int const value )
{
    char buffer[ 50 ];
    sprintf( buffer, "%i", value );
    return object_new( buffer );
}


OBJECT * outf_double( double const value )
{
    char buffer[ 50 ];
    sprintf( buffer, "%f", value );
    return object_new( buffer );
}


OBJECT * outf_time( timestamp const * const time )
{
    return object_new( timestamp_str( time ) );
}
