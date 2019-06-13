/*
 * Copyright 1993, 1995 Christopher Seiwald.
 * Copyright 2007 Noel Belcourt.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

#include "jam.h"
#include "execcmd.h"

#include "lists.h"
#include "output.h"
#include "strings.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>  /* vfork(), _exit(), STDOUT_FILENO and such */
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <poll.h>

#if defined(sun) || defined(__sun)
    #include <wait.h>
#endif

#ifdef USE_EXECUNIX

#include <sys/times.h>

#if defined(__APPLE__)
    #define NO_VFORK
#endif

#ifdef NO_VFORK
    #define vfork() fork()
#endif


/*
 * execunix.c - execute a shell script on UNIX/OS2/AmigaOS
 *
 * If $(JAMSHELL) is defined, uses that to formulate execvp()/spawnvp(). The
 * default is: /bin/sh -c
 *
 * In $(JAMSHELL), % expands to the command string and ! expands to the slot
 * number (starting at 1) for multiprocess (-j) invocations. If $(JAMSHELL) does
 * not include a %, it is tacked on as the last argument.
 *
 * Each word must be an individual element in a jam variable value.
 *
 * Do not just set JAMSHELL to /bin/sh - it will not work!
 *
 * External routines:
 *  exec_check() - preprocess and validate the command.
 *  exec_cmd() - launch an async command execution.
 *  exec_wait() - wait for any of the async command processes to terminate.
 */

/* find a free slot in the running commands table */
static int get_free_cmdtab_slot();

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static clock_t tps;

/* We hold stdout & stderr child process information in two element arrays
 * indexed as follows.
 */
#define OUT 0
#define ERR 1

static struct
{
    int          pid;            /* on win32, a real process handle */
    int          fd[ 2 ];        /* file descriptors for stdout and stderr */
    FILE      *  stream[ 2 ];    /* child's stdout and stderr file streams */
    clock_t      start_time;     /* start time of child process */
    int          exit_reason;    /* termination status */
    char      *  buffer[ 2 ];    /* buffers to hold stdout and stderr, if any */
    int          buf_size[ 2 ];  /* buffer sizes in bytes */
    timestamp    start_dt;       /* start of command timestamp */

    int flags;

    /* Function called when the command completes. */
    ExecCmdCallback func;

    /* Opaque data passed back to the 'func' callback. */
    void * closure;
} * cmdtab = NULL;
static int cmdtab_size = 0;

/* Contains both stdin and stdout of all processes.
 * The length is either globs.jobs or globs.jobs * 2
 * depending on globs.pipe_action.
 */
struct pollfd * wait_fds = NULL;
#define WAIT_FDS_SIZE ( globs.jobs * ( globs.pipe_action ? 2 : 1 ) )
#define GET_WAIT_FD( job_idx ) ( wait_fds + ( ( job_idx * ( globs.pipe_action ? 2 : 1 ) ) ) )

/*
 * exec_init() - global initialization
 */
void exec_init( void )
{
    int i;
    if ( globs.jobs > cmdtab_size )
    {
        cmdtab = BJAM_REALLOC( cmdtab, globs.jobs * sizeof( *cmdtab ) );
        memset( cmdtab + cmdtab_size, 0, ( globs.jobs - cmdtab_size ) * sizeof( *cmdtab ) );
        wait_fds = BJAM_REALLOC( wait_fds, WAIT_FDS_SIZE * sizeof ( *wait_fds ) );
        for ( i = cmdtab_size; i < globs.jobs; ++i )
        {
            GET_WAIT_FD( i )[ OUT ].fd = -1;
            GET_WAIT_FD( i )[ OUT ].events = POLLIN;
            if ( globs.pipe_action )
            {
                GET_WAIT_FD( i )[ ERR ].fd = -1;
                GET_WAIT_FD( i )[ ERR ].events = POLLIN;
            }
        }
        cmdtab_size = globs.jobs;
    }
}

void exec_done( void )
{
    BJAM_FREE( cmdtab );
    BJAM_FREE( wait_fds );
}

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
    int const is_raw_cmd = is_raw_command_request( *pShell );

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
 * exec_cmd() - launch an async command execution.
 */

/* We hold file descriptors for pipes used to communicate with child processes
 * in two element arrays indexed as follows.
 */
#define EXECCMD_PIPE_READ 0
#define EXECCMD_PIPE_WRITE 1

void exec_cmd
(
    string const * command,
    int flags,
    ExecCmdCallback func,
    void * closure,
    LIST * shell
)
{
    struct sigaction ignore, saveintr, savequit;
    sigset_t chldmask, savemask;

    int const slot = get_free_cmdtab_slot();
    int out[ 2 ];
    int err[ 2 ];
    int len;
    char const * argv[ MAXARGC + 1 ];  /* +1 for NULL */

    /* Initialize default shell. */
    static LIST * default_shell;
    if ( !default_shell )
        default_shell = list_push_back( list_new(
            object_new( "/bin/sh" ) ),
            object_new( "-c" ) );

    if ( list_empty( shell ) )
        shell = default_shell;

    /* Forumulate argv. If shell was defined, be prepared for % and ! subs.
     * Otherwise, use stock /bin/sh.
     */
    argv_from_shell( argv, shell, command->value, slot );

    if ( DEBUG_EXECCMD )
    {
        int i;
        out_printf( "Using shell: " );
        list_print( shell );
        out_printf( "\n" );
        for ( i = 0; argv[ i ]; ++i )
            out_printf( "    argv[%d] = '%s'\n", i, argv[ i ] );
    }

    /* Create pipes for collecting child output. */
    if ( pipe( out ) < 0 || ( globs.pipe_action && pipe( err ) < 0 ) )
    {
        perror( "pipe" );
        exit( EXITBAD );
    }

    /* Start the command */

    timestamp_current( &cmdtab[ slot ].start_dt );

    if ( 0 < globs.timeout )
    {
        /* Handle hung processes by manually tracking elapsed time and signal
         * process when time limit expires.
         */
        struct tms buf;
        cmdtab[ slot ].start_time = times( &buf );

        /* Make a global, only do this once. */
        if ( !tps ) tps = sysconf( _SC_CLK_TCK );
    }

    /* Child does not need the read pipe ends used by the parent. */
    fcntl( out[ EXECCMD_PIPE_READ ], F_SETFD, FD_CLOEXEC );
    if ( globs.pipe_action )
        fcntl( err[ EXECCMD_PIPE_READ ], F_SETFD, FD_CLOEXEC );

    /* ignore SIGINT and SIGQUIT */
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    if (sigaction(SIGINT, &ignore, &saveintr) < 0)
        return;
    if (sigaction(SIGQUIT, &ignore, &savequit) < 0)
        return;

    /* block SIGCHLD */
    sigemptyset(&chldmask);
    sigaddset(&chldmask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0)
        return;

    if ( ( cmdtab[ slot ].pid = vfork() ) == -1 )
    {
        perror( "vfork" );
        exit( EXITBAD );
    }

    if ( cmdtab[ slot ].pid == 0 )
    {
        /*****************/
        /* Child process */
        /*****************/
        int const pid = getpid();

        /* restore previous signals */
        sigaction(SIGINT, &saveintr, NULL);
        sigaction(SIGQUIT, &savequit, NULL);
        sigprocmask(SIG_SETMASK, &savemask, NULL);

        /* Redirect stdout and stderr to pipes inherited from the parent. */
        dup2( out[ EXECCMD_PIPE_WRITE ], STDOUT_FILENO );
        dup2( globs.pipe_action ? err[ EXECCMD_PIPE_WRITE ] :
            out[ EXECCMD_PIPE_WRITE ], STDERR_FILENO );
        close( out[ EXECCMD_PIPE_WRITE ] );
        if ( globs.pipe_action )
            close( err[ EXECCMD_PIPE_WRITE ] );

        /* Make this process a process group leader so that when we kill it, all
         * child processes of this process are terminated as well. We use
         * killpg( pid, SIGKILL ) to kill the process group leader and all its
         * children.
         */
        if ( 0 < globs.timeout )
        {
            struct rlimit r_limit;
            r_limit.rlim_cur = globs.timeout;
            r_limit.rlim_max = globs.timeout;
            setrlimit( RLIMIT_CPU, &r_limit );
        }
        if (0 != setpgid( pid, pid )) {
            perror("setpgid(child)");
            /* exit( EXITBAD ); */
        }
        execvp( argv[ 0 ], (char * *)argv );
        perror( "execvp" );
        _exit( 127 );
    }

    /******************/
    /* Parent process */
    /******************/

    /* redundant call, ignore return value */
    setpgid(cmdtab[ slot ].pid, cmdtab[ slot ].pid);

    /* Parent not need the write pipe ends used by the child. */
    close( out[ EXECCMD_PIPE_WRITE ] );
    if ( globs.pipe_action )
        close( err[ EXECCMD_PIPE_WRITE ] );

    /* Set both pipe read file descriptors to non-blocking. */
    fcntl( out[ EXECCMD_PIPE_READ ], F_SETFL, O_NONBLOCK );
    if ( globs.pipe_action )
        fcntl( err[ EXECCMD_PIPE_READ ], F_SETFL, O_NONBLOCK );

    /* Parent reads from out[ EXECCMD_PIPE_READ ]. */
    cmdtab[ slot ].fd[ OUT ] = out[ EXECCMD_PIPE_READ ];
    cmdtab[ slot ].stream[ OUT ] = fdopen( cmdtab[ slot ].fd[ OUT ], "rb" );
    if ( !cmdtab[ slot ].stream[ OUT ] )
    {
        perror( "fdopen" );
        exit( EXITBAD );
    }

    /* Parent reads from err[ EXECCMD_PIPE_READ ]. */
    if ( globs.pipe_action )
    {
        cmdtab[ slot ].fd[ ERR ] = err[ EXECCMD_PIPE_READ ];
        cmdtab[ slot ].stream[ ERR ] = fdopen( cmdtab[ slot ].fd[ ERR ], "rb" );
        if ( !cmdtab[ slot ].stream[ ERR ] )
        {
            perror( "fdopen" );
            exit( EXITBAD );
        }
    }

    GET_WAIT_FD( slot )[ OUT ].fd = out[ EXECCMD_PIPE_READ ];
    if ( globs.pipe_action )
        GET_WAIT_FD( slot )[ ERR ].fd = err[ EXECCMD_PIPE_READ ];

    cmdtab[ slot ].flags = flags;

    /* Save input data into the selected running commands table slot. */
    cmdtab[ slot ].func = func;
    cmdtab[ slot ].closure = closure;

    /* restore previous signals */
    sigaction(SIGINT, &saveintr, NULL);
    sigaction(SIGQUIT, &savequit, NULL);
    sigprocmask(SIG_SETMASK, &savemask, NULL);
}

#undef EXECCMD_PIPE_READ
#undef EXECCMD_PIPE_WRITE


/* Returns 1 if file descriptor is closed, or 0 if it is still alive.
 *
 * i is index into cmdtab
 *
 * s (stream) indexes:
 *  - cmdtab[ i ].stream[ s ]
 *  - cmdtab[ i ].buffer[ s ]
 *  - cmdtab[ i ].fd    [ s ]
 */

static int read_descriptor( int i, int s )
{
    int ret;
    char buffer[ BUFSIZ ];

    while ( 0 < ( ret = fread( buffer, sizeof( char ), BUFSIZ - 1,
        cmdtab[ i ].stream[ s ] ) ) )
    {
        buffer[ ret ] = 0;

        /* Copy it to our output if appropriate */
        if ( ! ( cmdtab[ i ].flags & EXEC_CMD_QUIET ) )
        {
            if ( s == OUT && ( globs.pipe_action != 2 ) )
                out_data( buffer );
            else if ( s == ERR && ( globs.pipe_action & 2 ) )
                err_data( buffer );
        }

        if ( !cmdtab[ i ].buffer[ s ] )
        {
            /* Never been allocated. */
            if ( globs.max_buf && ret > globs.max_buf )
            {
                ret = globs.max_buf;
                buffer[ ret ] = 0;
            }
            cmdtab[ i ].buf_size[ s ] = ret + 1;
            cmdtab[ i ].buffer[ s ] = (char*)BJAM_MALLOC_ATOMIC( ret + 1 );
            memcpy( cmdtab[ i ].buffer[ s ], buffer, ret + 1 );
        }
        else
        {
            /* Previously allocated. */
            if ( cmdtab[ i ].buf_size[ s ] < globs.max_buf || !globs.max_buf )
            {
                char * tmp = cmdtab[ i ].buffer[ s ];
                int const old_len = cmdtab[ i ].buf_size[ s ] - 1;
                int const new_len = old_len + ret + 1;
                cmdtab[ i ].buf_size[ s ] = new_len;
                cmdtab[ i ].buffer[ s ] = (char*)BJAM_MALLOC_ATOMIC( new_len );
                memcpy( cmdtab[ i ].buffer[ s ], tmp, old_len );
                memcpy( cmdtab[ i ].buffer[ s ] + old_len, buffer, ret + 1 );
                BJAM_FREE( tmp );
            }
        }
    }

    /* If buffer full, ensure last buffer char is newline so that jam log
     * contains the command status at beginning of it own line instead of
     * appended to end of the previous output.
     */
    if ( globs.max_buf && globs.max_buf <= cmdtab[ i ].buf_size[ s ] )
        cmdtab[ i ].buffer[ s ][ cmdtab[ i ].buf_size[ s ] - 2 ] = '\n';

    return feof( cmdtab[ i ].stream[ s ] );
}


/*
 * close_streams() - Close the stream and pipe descriptor.
 */

static void close_streams( int const i, int const s )
{
    fclose( cmdtab[ i ].stream[ s ] );
    cmdtab[ i ].stream[ s ] = 0;

    close( cmdtab[ i ].fd[ s ] );
    cmdtab[ i ].fd[ s ] = 0;

    GET_WAIT_FD( i )[ s ].fd = -1;
}


/*
 * exec_wait() - wait for any of the async command processes to terminate.
 *
 * May register more than one terminated child process but will exit as soon as
 * at least one has been registered.
 */

void exec_wait()
{
    int finished = 0;

    /* Process children that signaled. */
    while ( !finished )
    {
        int i;
        struct timeval tv;
        struct timeval * ptv = NULL;
        int select_timeout = globs.timeout;

        /* Check for timeouts:
         *   - kill children that already timed out
         *   - decide how long until the next one times out
         */
        if ( globs.timeout > 0 )
        {
            struct tms buf;
            clock_t const current = times( &buf );
            for ( i = 0; i < globs.jobs; ++i )
                if ( cmdtab[ i ].pid )
                {
                    clock_t const consumed =
                        ( current - cmdtab[ i ].start_time ) / tps;
                    if ( consumed >= globs.timeout )
                    {
                        killpg( cmdtab[ i ].pid, SIGKILL );
                        cmdtab[ i ].exit_reason = EXIT_TIMEOUT;
                    }
                    else if ( globs.timeout - consumed < select_timeout )
                        select_timeout = globs.timeout - consumed;
                }

            /* If nothing else causes our select() call to exit, force it after
             * however long it takes for the next one of our child processes to
             * crossed its allotted processing time so we can terminate it.
             */
            tv.tv_sec = select_timeout;
            tv.tv_usec = 0;
            ptv = &tv;
        }

        /* select() will wait for I/O on a descriptor, a signal, or timeout. */
        {
            /* disable child termination signals while in select */
            int ret;
            int timeout;
            sigset_t sigmask;
            sigemptyset(&sigmask);
            sigaddset(&sigmask, SIGCHLD);
            sigprocmask(SIG_BLOCK, &sigmask, NULL);

            /* If no timeout is specified, pass -1 (which means no timeout,
             * wait indefinitely) to poll, to prevent busy-looping.
             */
            timeout = select_timeout? select_timeout * 1000 : -1;
            while ( ( ret = poll( wait_fds, WAIT_FDS_SIZE, timeout ) ) == -1 )
                if ( errno != EINTR )
                    break;
            /* restore original signal mask by unblocking sigchld */
            sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
            if ( ret <= 0 )
                continue;
        }

        for ( i = 0; i < globs.jobs; ++i )
        {
            int out_done = 0;
            int err_done = 0;
            if ( GET_WAIT_FD( i )[ OUT ].revents )
                out_done = read_descriptor( i, OUT );

            if ( globs.pipe_action && ( GET_WAIT_FD( i )[ ERR ].revents ) )
                err_done = read_descriptor( i, ERR );

            /* If feof on either descriptor, we are done. */
            if ( out_done || err_done )
            {
                int pid;
                int status;
                int rstat;
                timing_info time_info;
                struct rusage cmd_usage;

                /* We found a terminated child process - our search is done. */
                finished = 1;

                /* Close the stream and pipe descriptors. */
                close_streams( i, OUT );
                if ( globs.pipe_action )
                    close_streams( i, ERR );

                /* Reap the child and release resources. */
                while ( ( pid = wait4( cmdtab[ i ].pid, &status, 0, &cmd_usage ) ) == -1 )
                    if ( errno != EINTR )
                        break;
                if ( pid != cmdtab[ i ].pid )
                {
                    err_printf( "unknown pid %d with errno = %d\n", pid, errno );
                    exit( EXITBAD );
                }

                /* Set reason for exit if not timed out. */
                if ( WIFEXITED( status ) )
                    cmdtab[ i ].exit_reason = WEXITSTATUS( status )
                        ? EXIT_FAIL
                        : EXIT_OK;

                {
                    time_info.system = ((double)(cmd_usage.ru_stime.tv_sec)*1000000.0+(double)(cmd_usage.ru_stime.tv_usec))/1000000.0;
                    time_info.user   = ((double)(cmd_usage.ru_utime.tv_sec)*1000000.0+(double)(cmd_usage.ru_utime.tv_usec))/1000000.0;
                    timestamp_copy( &time_info.start, &cmdtab[ i ].start_dt );
                    timestamp_current( &time_info.end );
                }

                /* Drive the completion. */
                if ( interrupted() )
                    rstat = EXEC_CMD_INTR;
                else if ( status )
                    rstat = EXEC_CMD_FAIL;
                else
                    rstat = EXEC_CMD_OK;

                /* Call the callback, may call back to jam rule land. */
                (*cmdtab[ i ].func)( cmdtab[ i ].closure, rstat, &time_info,
                    cmdtab[ i ].buffer[ OUT ], cmdtab[ i ].buffer[ ERR ],
                    cmdtab[ i ].exit_reason );

                /* Clean up the command's running commands table slot. */
                BJAM_FREE( cmdtab[ i ].buffer[ OUT ] );
                cmdtab[ i ].buffer[ OUT ] = 0;
                cmdtab[ i ].buf_size[ OUT ] = 0;

                BJAM_FREE( cmdtab[ i ].buffer[ ERR ] );
                cmdtab[ i ].buffer[ ERR ] = 0;
                cmdtab[ i ].buf_size[ ERR ] = 0;

                cmdtab[ i ].pid = 0;
                cmdtab[ i ].func = 0;
                cmdtab[ i ].closure = 0;
                cmdtab[ i ].start_time = 0;
            }
        }
    }
}


/*
 * Find a free slot in the running commands table.
 */

static int get_free_cmdtab_slot()
{
    int slot;
    for ( slot = 0; slot < globs.jobs; ++slot )
        if ( !cmdtab[ slot ].pid )
            return slot;
    err_printf( "no slots for child!\n" );
    exit( EXITBAD );
}

# endif /* USE_EXECUNIX */
