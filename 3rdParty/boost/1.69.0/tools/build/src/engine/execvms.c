/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*  This file is ALSO:
 *  Copyright 2001-2004 David Abrahams.
 *  Copyright 2015 Artur Shepilko.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */


/*
 * execvms.c - execute a shell script, ala VMS.
 *
 * The approach is this:
 *
 * If the command is a single line, and shorter than WRTLEN (what we believe to
 * be the maximum line length), we just system() it.
 *
 * If the command is multi-line, or longer than WRTLEN, we write the command
 * block to a temp file, splitting long lines (using "-" at the end of the line
 * to indicate contiuation), and then source that temp file. We use special
 * logic to make sure we do not continue in the middle of a quoted string.
 *
 * 05/04/94 (seiwald) - async multiprocess interface; noop on VMS
 * 12/20/96 (seiwald) - rewritten to handle multi-line commands well
 * 01/14/96 (seiwald) - do not put -'s between "'s
 * 01/19/15 (shepilko)- adapt for jam-3.1.19
 */

#include "jam.h"
#include "lists.h"
#include "execcmd.h"
#include "output.h"

#ifdef OS_VMS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <times.h>
#include <unistd.h>
#include <errno.h>


#define WRTLEN 240

#define MIN( a, b ) ((a) < (b) ? (a) : (b))

#define CHAR_DQUOTE  '"'

#define VMS_PATH_MAX 1024
#define VMS_COMMAND_MAX  1024

#define VMS_WARNING 0
#define VMS_SUCCESS 1
#define VMS_ERROR   2
#define VMS_FATAL   4

char commandbuf[ VMS_COMMAND_MAX ] = { 0 };


static int get_status(int vms_status);
static clock_t get_cpu_time();

/*
 * exec_check() - preprocess and validate the command.
 */

int exec_check
(
    string const * command,
    LIST * * pShell,
    int * error_length,
    int * error_max_length
)
{
    int const is_raw_cmd = 1;

    /* We allow empty commands for non-default shells since we do not really
     * know what they are going to do with such commands.
     */
    if ( !command->size && ( is_raw_cmd || list_empty( *pShell ) ) )
        return EXEC_CHECK_NOOP;

    return is_raw_cmd
        ? EXEC_CHECK_OK
        : check_cmd_for_too_long_lines( command->value, MAXLINE, error_length,
            error_max_length );
}


/*
 * exec_cmd() - execute system command.
 */

void exec_cmd
(
    string const * command,
    int flags,
    ExecCmdCallback func,
    void * closure,
    LIST * shell
)
{
    char * s;
    char * e;
    char * p;
    int vms_status;
    int status;
    int rstat = EXEC_CMD_OK;
    int exit_reason = EXIT_OK;
    timing_info time_info;
    timestamp start_dt;
    struct tms start_time;
    struct tms end_time;
    char * cmd_string = command->value;


    /* Start the command */

    timestamp_current( &time_info.start );
    times( &start_time );

    /* See if command is more than one line discounting leading/trailing white
     * space.
     */
    for ( s = cmd_string; *s && isspace( *s ); ++s );

    e = p = strchr( s, '\n' );

    while ( p && isspace( *p ) )
        ++p;

    /* If multi line or long, write to com file. Otherwise, exec directly. */
    if ( ( p && *p ) || ( e - s > WRTLEN ) )
    {
        FILE * f;

        /* Create temp file invocation. */

        if ( !*commandbuf )
        {
            OBJECT * tmp_filename = 0;

            tmp_filename = path_tmpfile();


            /* Get tmp file name is VMS-format. */
            {
                string os_filename[ 1 ];
                string_new( os_filename );
                path_translate_to_os( object_str( tmp_filename ), os_filename );
                object_free( tmp_filename );
                tmp_filename = object_new( os_filename->value );
                string_free( os_filename );
            }

            commandbuf[0] = '@';
            strncat( commandbuf + 1, object_str( tmp_filename ),
                     VMS_COMMAND_MAX - 2);
        }


        /* Open tempfile. */
        if ( !( f = fopen( commandbuf + 1, "w" ) ) )
        {
            printf( "can't open cmd_string file\n" );
            rstat = EXEC_CMD_FAIL;
            exit_reason = EXIT_FAIL;

            times( &end_time );

            timestamp_current( &time_info.end );
            time_info.system = (double)( end_time.tms_cstime -
                start_time.tms_cstime ) / 100.;
            time_info.user   = (double)( end_time.tms_cutime -
                start_time.tms_cutime ) / 100.;

            (*func)( closure, rstat, &time_info, "" , "", exit_reason  );
            return;
        }


        /* Running from TMP, so explicitly set default to CWD. */
        {
            char * cwd = NULL;
            int cwd_buf_size = VMS_PATH_MAX;

            while ( !(cwd = getcwd( NULL, cwd_buf_size ) ) /* alloc internally */
                     && errno == ERANGE )
            {
                cwd_buf_size += VMS_PATH_MAX;
            }

            if ( !cwd )
            {
                perror( "can not get current working directory" );
                exit( EXITBAD );
            }

            fprintf( f, "$ SET DEFAULT %s\n", cwd);

            free( cwd );
        }


        /* For each line of the command. */
        while ( *cmd_string )
        {
            char * s = strchr( cmd_string,'\n' );
            int len = s ? s + 1 - cmd_string : strlen( cmd_string );

            fputc( '$', f );

            /* For each chunk of a line that needs to be split. */
            while ( len > 0 )
            {
                char * q = cmd_string;
                char * qe = cmd_string + MIN( len, WRTLEN );
                char * qq = q;
                int quote = 0;

                /* Look for matching "s -- expected in the same line. */
                for ( ; q < qe; ++q )
                    if ( ( *q == CHAR_DQUOTE ) && ( quote = !quote ) )
                        qq = q;

                /* When needs splitting and is inside an open quote,
                 * back up to opening quote and split off at it.
                 * When the quoted string spans over a chunk,
                 * pass string as a whole.
                 * If no matching quote found, dump the rest of command.
                 */
                if (  len > WRTLEN && quote )
                {
                    q = qq;

                    if ( q == cmd_string )
                    {
                        for ( q = qe; q < ( cmd_string + len )
                                      && *q != CHAR_DQUOTE ; ++q) {}
                        q = ( *q == CHAR_DQUOTE) ? ( q + 1 ) : ( cmd_string + len );
                    }
                }

                fwrite( cmd_string, ( q - cmd_string ), 1, f );

                len -= ( q - cmd_string );
                cmd_string = q;

                if ( len )
                {
                    fputc( '-', f );
                    fputc( '\n', f );
                }
            }
        }

        fclose( f );

        if ( DEBUG_EXECCMD )
        {
            FILE * f;
            char buf[ WRTLEN + 1 ] = { 0 };

            if ( (f = fopen( commandbuf + 1, "r" ) ) )
            {
                int nbytes;
                printf( "Command file: %s\n", commandbuf + 1 );

                do
                {
                    nbytes = fread( buf, sizeof( buf[0] ), sizeof( buf ) - 1, f );

                    if ( nbytes ) fwrite(buf, sizeof( buf[0] ), nbytes, stdout);
                }
                while ( !feof(f) );

                fclose(f);
            }
        }

        /* Execute command file */
        vms_status = system( commandbuf );
        status = get_status( vms_status );

        unlink( commandbuf + 1 );
    }
    else
    {
        /* Execute single line command. Strip trailing newline before execing.
         * TODO:Call via popen() with capture of the output may be better here.
         */
        if ( e ) *e = 0;

        status = VMS_SUCCESS;  /* success on empty command */
        if ( *s )
        {
            vms_status = system( s );
            status = get_status( vms_status );
        }
    }


    times( &end_time );

    timestamp_current( &time_info.end );
    time_info.system = (double)( end_time.tms_cstime -
        start_time.tms_cstime ) / 100.;
    time_info.user   = (double)( end_time.tms_cutime -
        start_time.tms_cutime ) / 100.;


    /* Fail for error or fatal error. OK on OK, warning or info exit. */
    if ( ( status == VMS_ERROR ) || ( status == VMS_FATAL ) )
    {
        rstat = EXEC_CMD_FAIL;
        exit_reason = EXIT_FAIL;
    }

    (*func)( closure, rstat, &time_info, "" , "", exit_reason  );
}


void exec_wait()
{
    return;
}


/* get_status() - returns status of the VMS command execution.
   - Map VMS status to its severity (lower 3-bits)
   - W-DCL-IVVERB is returned on unrecognized command -- map to general ERROR
*/
int get_status( int vms_status )
{
#define VMS_STATUS_DCL_IVVERB 0x00038090

    int status;

    switch (vms_status)
    {
    case VMS_STATUS_DCL_IVVERB:
        status = VMS_ERROR;
        break;

    default:
        status = vms_status & 0x07; /* $SEVERITY bits */
    }

    return status;
}


#define __NEW_STARLET 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ssdef.h>
#include <stsdef.h>
#include <jpidef.h>
#include <efndef.h>
#include <iosbdef.h>
#include <iledef.h>
#include <lib$routines.h>
#include <starlet.h>


/*
 * get_cpu_time() - returns CPU time in CLOCKS_PER_SEC since process start.
 * on error returns (clock_t)-1.
 *
 * Intended to emulate (system + user) result of *NIX times(), if CRTL times()
 * is not available.
*  However, this accounts only for the current process. To account for child
*  processes, these need to be directly spawned/forked via exec().
*  Moreover, child processes should be running a C main program or a program
*  that calls VAXC$CRTL_INIT or DECC$CRTL_INIT.
*/

clock_t get_cpu_time()
{
    clock_t result = (clock_t) 0;

    IOSB iosb;
    int status;
    long cputime = 0;


    ILE3 jpi_items[] = {
        { sizeof( cputime ), JPI$_CPUTIM, &cputime, NULL }, /* longword int, 10ms */
        { 0 },
    };

    status = sys$getjpiw (EFN$C_ENF, 0, 0, jpi_items, &iosb, 0, 0);

    if ( !$VMS_STATUS_SUCCESS( status ) )
    {
        lib$signal( status );

        result = (clock_t) -1;
        return result;
    }


    result = ( cputime / 100 ) * CLOCKS_PER_SEC;

    return result;
}


# endif /* VMS */

