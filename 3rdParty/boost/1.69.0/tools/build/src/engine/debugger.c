/*
 * Copyright 2015 Steven Watanabe
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include "debugger.h"
#include "constants.h"
#include "strings.h"
#include "pathsys.h"
#include "cwd.h"
#include "function.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>

#ifdef NT
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#undef debug_on_enter_function
#undef debug_on_exit_function

struct breakpoint
{
    OBJECT * file;
    OBJECT * bound_file;
    int line;
    int status;
};

#define BREAKPOINT_ENABLED 1
#define BREAKPOINT_DISABLED 2
#define BREAKPOINT_DELETED 3

static struct breakpoint * breakpoints;
static int num_breakpoints;
static int breakpoints_capacity;

#define DEBUG_NO_CHILD  0
#define DEBUG_RUN       1
#define DEBUG_STEP      2
#define DEBUG_NEXT      3
#define DEBUG_FINISH    4
#define DEBUG_STOPPED   5

#define DEBUG_MSG_BREAKPOINT   1
#define DEBUG_MSG_END_STEPPING 2
#define DEBUG_MSG_SETUP        3
#define DEBUG_MSG_DONE         32

static int debug_state;
static int debug_depth;
static OBJECT * debug_file;
static int debug_line;
static FRAME * debug_frame;
LIST * debug_print_result;
static int current_token;
static int debug_selected_frame_number;

/* Commands are read from this stream. */
static FILE * command_input;
/* Where to send output from commands. */
static FILE * command_output;
/* Only valid in the parent.  Reads command output from the child. */
static FILE * command_child;

struct command_elem
{
    const char * key;
    void (*command)( int, const char * * );
};

static struct command_elem * command_array;

static void debug_listen( void );
static int read_command( void );
static int is_same_file( OBJECT * file1, OBJECT * file2 );
static void debug_mi_format_token( void );
static OBJECT * make_absolute_path( OBJECT * filename );

static void debug_string_write( FILE * out, const char * data )
{
    fprintf( out, "%s", data );
    fputc( '\0', out );
}

static char * debug_string_read( FILE * in )
{
    string buf[ 1 ];
    int ch;
    char * result;
    string_new( buf );
    while( ( ch = fgetc( in ) ) > 0 )
    {
        string_push_back( buf, (char)ch );
    }
    result = strdup( buf->value );
    string_free( buf );
    return result;
}

static void debug_object_write( FILE * out, OBJECT * data )
{
    debug_string_write( out, object_str( data ) );
}

static OBJECT * debug_object_read( FILE * in )
{
    string buf[ 1 ];
    int ch;
    OBJECT * result;
    string_new( buf );
    while( ( ch = fgetc( in ) ) > 0 )
    {
        string_push_back( buf, (char)ch );
    }
    result = object_new( buf->value );
    string_free( buf );
    return result;
}

static void debug_int_write( FILE * out, int i )
{
    fprintf( out, "%d", i );
    fputc( '\0', out );
}

static int debug_int_read( FILE * in )
{
    OBJECT * str = debug_object_read( in );
    int result = atoi( object_str( str ) );
    object_free( str );
    return result;
}

static void debug_list_write( FILE * out, LIST * l )
{
    LISTITER iter = list_begin( l ), end = list_end( l );
    fprintf( out, "%d\n", list_length( l ) );
    for ( ; iter != end; iter = list_next( iter ) )
    {
        debug_object_write( out, list_item( iter ) );
    }
}

static LIST * debug_list_read( FILE * in )
{
    int len;
    int i;
    int ch;
    LIST * result = L0;
    fscanf( in, "%d", &len );
    ch = fgetc( in );
    assert( ch == '\n' );
    for ( i = 0; i < len; ++i )
    {
        result = list_push_back( result, debug_object_read( in ) );
    }
    return result;
}

static void debug_lol_write( FILE * out, LOL * lol )
{
    int i;
    debug_int_write( out, lol->count );
    for ( i = 0; i < lol->count; ++i )
    {
        debug_list_write( out, lol_get( lol, i ) );
    }
}

static void debug_lol_read( FILE * in, LOL * lol )
{
    int count, i;
    lol_init( lol );
    count = debug_int_read( in );
    for ( i = 0; i < count; ++i )
    {
        lol_add( lol, debug_list_read( in ) );
    }
}

static void debug_format_rulename ( string * out, FRAME * frame )
{
    const char * pos = strchr( frame->rulename, '.' );
    if ( frame->module->class_module && pos )
    {
        string_copy( out, object_str( frame->module->name ) );
        string_push_back( out, '.' );
        string_append( out, pos + 1 );
    }
    else
    {
        string_copy( out, frame->rulename );
    }
}

static void debug_frame_write( FILE * out, FRAME * frame )
{
    string rulename_buffer [ 1 ];
    OBJECT * fullname = constant_builtin;
    OBJECT * file = frame->file;
    if ( file == NULL ) file = constant_builtin;
    else fullname = make_absolute_path( frame->file );
    debug_format_rulename( rulename_buffer, frame );
    debug_object_write( out, file );
    debug_int_write( out, frame->line );
    debug_object_write( out, fullname );
    debug_lol_write( out, frame->args );
    debug_string_write( out, rulename_buffer->value );
    object_free( fullname );
    string_free( rulename_buffer );
}

/*
 * The information passed to the debugger for
 * a frame is slightly different from the FRAME
 * struct.
 */
typedef struct _frame_info
{
    OBJECT * file;
    int line;
    OBJECT * fullname;
    LOL args[ 1 ];
    char * rulename;
} FRAME_INFO;

static void debug_frame_info_free( FRAME_INFO * frame )
{
    object_free( frame->file );
    object_free( frame->fullname );
    lol_free( frame->args );
    free( frame->rulename );
}

static void debug_frame_read( FILE * in, FRAME_INFO * frame )
{
    frame->file = debug_object_read( in );
    frame->line = debug_int_read( in );
    frame->fullname = debug_object_read( in );
    debug_lol_read( in, frame->args );
    frame->rulename = debug_string_read( in );
}

static int add_breakpoint( struct breakpoint elem )
{
    if ( num_breakpoints == breakpoints_capacity )
    {
        int new_capacity = breakpoints_capacity * 2;
        if ( new_capacity == 0 ) new_capacity = 1;
        breakpoints = ( struct breakpoint * )realloc( breakpoints, new_capacity * sizeof( struct breakpoint ) );
        breakpoints_capacity = new_capacity;
    }
    breakpoints[ num_breakpoints++ ] = elem;
    return num_breakpoints;
}

static int add_line_breakpoint( OBJECT * file, int line )
{
    struct breakpoint elem;
    elem.file = file;
    elem.bound_file = NULL;
    elem.line = line;
    elem.status = BREAKPOINT_ENABLED;
    return add_breakpoint( elem );
}

static int add_function_breakpoint( OBJECT * name )
{
    struct breakpoint elem;
    elem.file = name;
    elem.bound_file = object_copy( name );
    elem.line = -1;
    elem.status = BREAKPOINT_ENABLED;
    return add_breakpoint( elem );
}

/*
 * Checks whether there is an active breakpoint at the
 * specified location.  Returns the breakpoint id
 * or -1 if none is found.
 */
static int handle_line_breakpoint( OBJECT * file, int line )
{
    int i;
    if ( file == NULL ) return 0;
    for ( i = 0; i < num_breakpoints; ++i )
    {
        if ( breakpoints[ i ].bound_file == NULL && is_same_file( breakpoints[ i ].file, file ) )
        {
            breakpoints[ i ].bound_file = object_copy( file );
        }
        if ( breakpoints[ i ].status == BREAKPOINT_ENABLED &&
            breakpoints[ i ].bound_file != NULL &&
            object_equal( breakpoints[ i ].bound_file, file ) &&
            breakpoints[ i ].line == line )
        {
            return i + 1;
        }
    }
    return 0;
}

static int handle_function_breakpoint( OBJECT * name )
{
    return handle_line_breakpoint( name, -1 );
}

static OBJECT * make_absolute_path( OBJECT * filename )
{
    PATHNAME path1[ 1 ];
    string buf[ 1 ];
    OBJECT * result;
    const char * root = object_str( cwd() );
    path_parse( object_str( filename ), path1 );
    path1->f_root.ptr = root;
    path1->f_root.len = strlen( root );
    string_new( buf );
    path_build( path1, buf );
    result = object_new( buf->value );
    string_free( buf );
    return result;
}

static OBJECT * get_filename( OBJECT * path )
{
    PATHNAME path1[ 1 ];
    string buf[ 1 ];
    OBJECT * result;
    const char * root = object_str( cwd() );
    path_parse( object_str( path ), path1 );
    path1->f_dir.ptr = NULL;
    path1->f_dir.len = 0;
    string_new( buf );
    path_build( path1, buf );
    result = object_new( buf->value );
    string_free( buf );
    return result;
}

static int is_same_file( OBJECT * file1, OBJECT * file2 )
{
    OBJECT * absolute1 = make_absolute_path( file1 );
    OBJECT * absolute2 = make_absolute_path( file2 );
    OBJECT * norm1 = path_as_key( absolute1 );
    OBJECT * norm2 = path_as_key( absolute2 );
    OBJECT * base1 = get_filename( file1 );
    OBJECT * base2 = get_filename( file2 );
    OBJECT * normbase1 = path_as_key( base1 );
    OBJECT * normbase2 = path_as_key( base2 );
    int result = object_equal( norm1, norm2 ) ||
        ( object_equal( base1, file1 ) && object_equal( normbase1, normbase2 ) );
    object_free( absolute1 );
    object_free( absolute2 );
    object_free( norm1 );
    object_free( norm2 );
    object_free( base1 );
    object_free( base2 );
    object_free( normbase1 );
    object_free( normbase2 );
    return result;
}

static void debug_print_source( OBJECT * filename, int line )
{
    FILE * file;

    if ( filename == NULL || object_equal( filename, constant_builtin ) )
        return;

    file = fopen( object_str( filename ), "r" );
    if ( file )
    {
        int ch;
        int printing = 0;
        int current_line = 1;
        if ( line == 1 )
        {
            printing = 1;
            printf( "%d\t", current_line );
        }
        while ( ( ch = fgetc( file ) ) != EOF )
        {
            if ( printing )
                fputc( ch, stdout );

            if ( ch == '\n' )
            {
                if ( printing )
                    break;

                ++current_line;
                if ( current_line == line )
                {
                    printing = 1;
                    printf( "%d\t", current_line );
                }
            }
        }
        fclose( file );
    }
}

static void debug_print_frame( FRAME * frame )
{
    OBJECT * file = frame->file;
    if ( file == NULL ) file = constant_builtin;
    printf( "%s ", frame->rulename );
    if ( strcmp( frame->rulename, "module scope" ) != 0 )
    {
        printf( "( " );
        if ( frame->args->count )
        {
            lol_print( frame->args );
            printf( " " );
        }
        printf( ") " );
    }
    printf( "at %s:%d", object_str( file ), frame->line );
}

static void debug_mi_print_frame( FRAME * frame )
{
    OBJECT * fullname = make_absolute_path( frame->file );
    printf( "frame={func=\"%s\",args=[],file=\"%s\",fullname=\"%s\",line=\"%d\"}",
        frame->rulename,
        object_str( frame->file ),
        object_str( fullname ),
        frame->line );
    object_free( fullname );
}

static void debug_print_frame_info( FRAME_INFO * frame )
{
    OBJECT * file = frame->file;
    if ( file == NULL ) file = constant_builtin;
    printf( "%s ", frame->rulename );
    if ( strcmp( frame->rulename, "module scope" ) != 0 )
    {
        printf( "( " );
        if ( frame->args->count )
        {
            lol_print( frame->args );
            printf( " " );
        }
        printf( ") " );
    }
    printf( "at %s:%d", object_str( file ), frame->line );
}

static void debug_mi_print_frame_info( FRAME_INFO * frame )
{
    printf( "frame={func=\"%s\",args=[],file=\"%s\",fullname=\"%s\",line=\"%d\"}",
        frame->rulename,
        object_str( frame->file ),
        object_str( frame->fullname ),
        frame->line );
}

static void debug_on_breakpoint( int id )
{
    fputc( DEBUG_MSG_BREAKPOINT, command_output );
    debug_int_write( command_output, id );
    fflush( command_output );
    debug_listen();
}

static void debug_end_stepping( void )
{
    fputc( DEBUG_MSG_END_STEPPING, command_output );
    fflush( command_output );
    debug_listen();
}

void debug_on_instruction( FRAME * frame, OBJECT * file, int line )
{
    int breakpoint_id;
    assert( debug_is_debugging() );
    if ( debug_state == DEBUG_NEXT &&
        ( debug_depth < 0 || ( debug_depth == 0 && debug_line != line ) ) )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_end_stepping();
    }
    else if ( debug_state == DEBUG_STEP && debug_line != line )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_end_stepping();
    }
    else if ( debug_state == DEBUG_FINISH && debug_depth < 0 )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_end_stepping();
    }
    else if ( ( debug_file == NULL || ! object_equal( file, debug_file ) ||
                line != debug_line || debug_depth != 0 ) &&
        ( breakpoint_id = handle_line_breakpoint( file, line ) ) )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_on_breakpoint( breakpoint_id );
    }
    else if ( ( debug_state == DEBUG_RUN || debug_state == DEBUG_FINISH ) &&
        ( debug_depth < 0 || ( debug_depth == 0 && debug_line != line ) ) )
    {
        debug_file = NULL;
        debug_line = 0;
    }
}

void debug_on_enter_function( FRAME * frame, OBJECT * name, OBJECT * file, int line )
{
    int breakpoint_id;
    assert( debug_is_debugging() );
    ++debug_depth;
    if ( debug_state == DEBUG_STEP && file )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_end_stepping();
    }
    else if ( ( breakpoint_id = handle_function_breakpoint( name ) ) ||
        ( breakpoint_id = handle_line_breakpoint( file, line ) ) )
    {
        debug_file = file;
        debug_line = line;
        debug_frame = frame;
        debug_on_breakpoint( breakpoint_id );
    }
}

void debug_on_exit_function( OBJECT * name )
{
    assert( debug_is_debugging() );
    --debug_depth;
    if ( debug_depth < 0 )
    {
        /* The current location is no longer valid
           after we return from the containing function. */
        debug_file = NULL;
        debug_line = 0;
    }
}

#if NT
static HANDLE child_handle;
static DWORD child_pid;
#else
static int child_pid;
#endif

static void debug_child_continue( int argc, const char * * argv )
{
    debug_state = DEBUG_RUN;
    debug_depth = 0;
}

static void debug_child_step( int argc, const char * * argv )
{
    debug_state = DEBUG_STEP;
    debug_depth = 0;
}

static void debug_child_next( int argc, const char * * argv )
{
    debug_state = DEBUG_NEXT;
    debug_depth = 0;
}

static void debug_child_finish( int argc, const char * * argv )
{
    debug_state = DEBUG_FINISH;
    debug_depth = 0;
}

static void debug_child_kill( int argc, const char * * argv )
{
    exit( 0 );
}

static int debug_add_breakpoint( const char * name )
{
    const char * file_ptr = name;
    const char * ptr = strrchr( file_ptr, ':' );
    if ( ptr )
    {
        char * end;
        long line = strtoul( ptr + 1, &end, 10 );
        if ( line > 0 && line <= INT_MAX && end != ptr + 1 && *end == 0 )
        {
            OBJECT * file = object_new_range( file_ptr,  ptr - file_ptr );
            return add_line_breakpoint( file, line );
        }
        else
        {
            OBJECT * name = object_new( file_ptr );
            return add_function_breakpoint( name );
        }
    }
    else
    {
        OBJECT * name = object_new( file_ptr );
        return add_function_breakpoint( name );
    }
}

static void debug_child_break( int argc, const char * * argv )
{
    if ( argc == 2 )
    {
        debug_add_breakpoint( argv[ 1 ] );
    }
}

static int get_breakpoint_by_name( const char * name )
{
    int result;
    const char * file_ptr = name;
    const char * ptr = strrchr( file_ptr, ':' );
    if ( ptr )
    {
        char * end;
        long line = strtoul( ptr + 1, &end, 10 );
        if ( line > 0 && line <= INT_MAX && end != ptr + 1 && *end == 0 )
        {
            OBJECT * file = object_new_range( file_ptr,  ptr - file_ptr );
            result = handle_line_breakpoint( file, line );
            object_free( file );
        }
        else
        {
            OBJECT * name = object_new( file_ptr );
            result = handle_function_breakpoint( name );
            object_free( name );
        }
    }
    else
    {
        OBJECT * name = object_new( file_ptr );
        result = handle_function_breakpoint( name );
        object_free( name );
    }
    return result;
}

static void debug_child_disable( int argc, const char * * argv )
{
    if ( argc == 2 )
    {
        int id = atoi( argv[ 1 ] );
        if ( id < 1 || id > num_breakpoints )
            return;
        --id;
        if ( breakpoints[ id ].status == BREAKPOINT_DELETED )
            return;
        breakpoints[ id ].status = BREAKPOINT_DISABLED;
    }
}

static void debug_child_enable( int argc, const char * * argv )
{
    if ( argc == 2 )
    {
        int id = atoi( argv[ 1 ] );
        if ( id < 1 || id > num_breakpoints )
            return;
        --id;
        if ( breakpoints[ id ].status == BREAKPOINT_DELETED )
            return;
        breakpoints[ id ].status = BREAKPOINT_ENABLED;
    }
}

static void debug_child_delete( int argc, const char * * argv )
{
    if ( argc == 2 )
    {
        int id = atoi( argv[ 1 ] );
        if ( id < 1 || id > num_breakpoints )
            return;
        --id;
        breakpoints[ id ].status = BREAKPOINT_DELETED;
    }
}

static void debug_child_print( int argc, const char * * argv )
{
    FRAME * saved_frame;
    OBJECT * saved_file;
    int saved_line;
    string buf[ 1 ];
    const char * lines[ 2 ];
    int i;
    FRAME new_frame = *debug_frame;
    /* Save the current file/line, since running parse_string
     * will likely change it.
     */
    saved_frame = debug_frame;
    saved_file = debug_file;
    saved_line = debug_line;
    string_new( buf );
    string_append( buf, "__DEBUG_PRINT_HELPER__" );
    for ( i = 1; i < argc; ++i )
    {
        string_push_back( buf, ' ' );
        string_append( buf, argv[ i ] );
    }
    string_append( buf, " ;\n" );
    lines[ 0 ] = buf->value;
    lines[ 1 ] = NULL;
    parse_string( constant_builtin, lines, &new_frame );
    string_free( buf );
    debug_list_write( command_output, debug_print_result );
    fflush( command_output );
    debug_frame = saved_frame;
    debug_file = saved_file;
    debug_line = saved_line;
}

static void debug_child_frame( int argc, const char * * argv )
{
    if ( argc == 2 )
    {
        debug_selected_frame_number = atoi( argv[ 1 ] );
    }
    else
    {
        assert( !"Wrong number of arguments to frame." );
    }
}

static void debug_child_info( int argc, const char * * argv )
{
    if ( strcmp( argv[ 1 ], "locals" ) == 0 )
    {
        LIST * locals = L0;
        if ( debug_frame->function )
        {
            locals = function_get_variables( debug_frame->function );
        }
        debug_list_write( command_output, locals );
        fflush( command_output );
        list_free( locals );
    }
    else if ( strcmp( argv[ 1 ], "frame" ) == 0 )
    {
        int frame_number = debug_selected_frame_number;
        int i;
        OBJECT * fullname;
        FRAME base = *debug_frame;
        FRAME * frame = &base;
        base.file = debug_file;
        base.line = debug_line;
        if ( argc == 3 ) frame_number = atoi( argv[ 2 ] );

        for ( i = 0; i < frame_number; ++i ) frame = frame->prev;

        debug_frame_write( command_output, frame );
    }
    else if ( strcmp( argv[ 1 ], "depth" ) == 0 )
    {
        int result = 0;
        FRAME * frame = debug_frame;
        while ( frame )
        {
            frame = frame->prev;
            ++result;
        }
        fprintf( command_output, "%d", result );
        fputc( '\0', command_output );
        fflush( command_output );
    }
}

/* Commands for the parent. */

#ifdef NT

static int get_module_filename( string * out )
{
    DWORD result;
    string_reserve( out, 256 + 1 );
    string_truncate( out, 256 );
    while( ( result = GetModuleFileName( NULL, out->value, out->size ) ) == out->size )
    {
        string_reserve( out, out->size * 2 + 1);
        string_truncate( out, out->size * 2 );
    }
    if ( result != 0 )
    {
        string_truncate( out, result );
        return 1;
    }
    else
    {
        return 0;
    }
}

#endif

static struct command_elem child_commands[] =
{
    { "continue", &debug_child_continue },
    { "kill", &debug_child_kill },
    { "step", &debug_child_step },
    { "next", &debug_child_next },
    { "finish", &debug_child_finish },
    { "break", &debug_child_break },
    { "disable", &debug_child_disable },
    { "enable", &debug_child_enable },
    { "delete", &debug_child_delete },
    { "print", &debug_child_print },
    { "frame", &debug_child_frame },
    { "info", &debug_child_info },
    { NULL, NULL }
};

static void debug_mi_error( const char * message )
{
    debug_mi_format_token();
    printf( "^error,msg=\"%s\"\n(gdb) \n", message );
}

static void debug_error_( const char * message )
{
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "%s\n", message );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_error( message );
    }
}

static const char * debug_format_message( const char * format, va_list vargs )
{
    char * buf;
    int result;
    int sz = 80;
    for ( ; ; )
    {
        va_list args;
        buf = malloc( sz );
        if ( !buf )
            return 0;
        #ifndef va_copy
        args = vargs;
        #else
        va_copy( args, vargs );
        #endif
        #if defined(_MSC_VER) && (_MSC_VER <= 1310)
        result = _vsnprintf( buf, sz, format, args );
        #else
        result = vsnprintf( buf, sz, format, args );
        #endif
        va_end( args );
        if ( 0 <= result && result < sz )
	    return buf;
        free( buf );
        if ( result < 0 )
            return 0;
        sz = result + 1;
    }
}

static void debug_error( const char * format, ... )
{
    va_list args;
    const char * msg;
    va_start( args, format );
    msg = debug_format_message( format, args );
    va_end( args );
    if ( !msg )
    {
        debug_error_( "Failed formatting error message." );
        return;
    }
    debug_error_( msg );
    free( ( void * )msg );
}

static void debug_parent_child_exited( int pid, int exit_code )
{
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "Child %d exited with status %d\n", (int)child_pid, (int)exit_code );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        if ( exit_code == 0 )
            printf( "*stopped,reason=\"exited-normally\"\n(gdb) \n" );
        else
            printf( "*stopped,reason=\"exited\",exit-code=\"%d\"\n(gdb) \n", exit_code );
    }
    else
    {
        assert( !"Wrong value of debug_interface." );
    }
}

static void debug_parent_child_signalled( int pid, int sigid )
{
    
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "Child %d exited on signal %d\n", child_pid, sigid );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        const char * name = "unknown";
        const char * meaning = "unknown";
        switch( sigid )
        {
        case SIGINT: name = "SIGINT"; meaning = "Interrupt"; break;
        }
        printf("*stopped,reason=\"exited-signalled\",signal-name=\"%s\",signal-meaning=\"%s\"\n(gdb) \n", name, meaning);
    }
    else
    {
        assert( !"Wrong value of debug_interface." );
    }
}

static void debug_parent_on_breakpoint( void )
{
    FRAME_INFO base;
    int id;
    id = debug_int_read( command_child );
    fprintf( command_output, "info frame\n" );
    fflush( command_output );
    debug_frame_read( command_child, &base );
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "Breakpoint %d, ", id );
        debug_print_frame_info( &base );
        printf( "\n" );
        debug_print_source( base.file, base.line );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        printf( "*stopped,reason=\"breakpoint-hit\",bkptno=\"%d\",disp=\"keep\",", id );
        debug_mi_print_frame_info( &base );
        printf( ",thread-id=\"1\",stopped-threads=\"all\"" );
        printf( "\n(gdb) \n" );
    }
    else
    {
        assert( !"Wrong value if debug_interface" );
    }
    fflush( stdout );
}

static void debug_parent_on_end_stepping( void )
{
    FRAME_INFO base;
    fprintf( command_output, "info frame\n" );
    fflush( command_output );
    debug_frame_read( command_child, &base );
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        debug_print_source( base.file, base.line );
    }
    else
    {
        printf( "*stopped,reason=\"end-stepping-range\"," );
        debug_mi_print_frame_info( &base );
        printf( ",thread-id=\"1\"" );
        printf( "\n(gdb) \n" );
    }
    fflush( stdout );
}

/* Waits for events from the child. */
static void debug_parent_wait( int print_message )
{
    int ch = fgetc( command_child );
    if ( ch == DEBUG_MSG_BREAKPOINT )
    {
        debug_parent_on_breakpoint();
    }
    else if ( ch == DEBUG_MSG_END_STEPPING )
    {
        debug_parent_on_end_stepping();
    }
    else if ( ch == DEBUG_MSG_SETUP )
    {
        /* FIXME: This is handled in the caller, but it would make
           more sense to handle it here. */
        return;
    }
    else if ( ch == EOF )
    {
#if NT
        WaitForSingleObject( child_handle, INFINITE );
        if ( print_message )
        {
            DWORD exit_code;
            GetExitCodeProcess( child_handle, &exit_code );
            debug_parent_child_exited( (int)child_pid, (int)exit_code );
        }
        CloseHandle( child_handle );
#else
        int status;
        int pid;
        while ( ( pid = waitpid( child_pid, &status, 0 ) ) == -1 )
            if ( errno != EINTR )
                break;
        if ( print_message )
        {
            if ( WIFEXITED( status ) )
                debug_parent_child_exited( child_pid, WEXITSTATUS( status ) );
            else if ( WIFSIGNALED( status ) )
                debug_parent_child_signalled( child_pid, WTERMSIG( status ) );
        }
#endif
        fclose( command_child );
        fclose( command_output );
        debug_state = DEBUG_NO_CHILD;
    }
}

/* Prints the message for starting the child. */
static void debug_parent_run_print( int argc, const char * * argv )
{
    int i;
    extern char const * saved_argv0;
    char * name = executable_path( saved_argv0 );
    printf( "Starting program: %s", name );
    free( name );
    for ( i = 1; i < argc; ++i )
    {
        printf( " %s", argv[ i ] );
    }
    printf( "\n" );
    fflush( stdout );
}

#if NT

void debug_init_handles( const char * in, const char * out )
{
    HANDLE read_handle;
    int read_fd;
    HANDLE write_handle;
    int write_fd;

    sscanf( in, "%p", &read_handle );
    read_fd = _open_osfhandle( (intptr_t)read_handle, _O_RDONLY );
    command_input = _fdopen( read_fd, "r" );
    
    sscanf( out, "%p", &write_handle );
    write_fd = _open_osfhandle( (intptr_t)write_handle, _O_WRONLY );
    command_output = _fdopen( write_fd, "w" );

    command_array = child_commands;

    /* Handle the initial setup */
    /* wake up the parent */
    fputc( DEBUG_MSG_SETUP, command_output );
    debug_listen();
}

static void init_parent_handles( HANDLE out, HANDLE in )
{
    int read_fd, write_fd;

    command_child = _fdopen( _open_osfhandle( (intptr_t)in, _O_RDONLY ), "r" );
    command_output = _fdopen( _open_osfhandle( (intptr_t)out, _O_WRONLY ), "w" );
}

static void debug_parent_copy_breakpoints( void )
{
    int i;
    for ( i = 0; i < num_breakpoints; ++i )
    {
        fprintf( command_output, "break %s", object_str( breakpoints[ i ].file ) );
        if ( breakpoints[ i ].line != -1 )
        {
            fprintf( command_output, ":%d", breakpoints[ i ].line );
        }
        fprintf( command_output, "\n" );

        switch ( breakpoints[ i ].status )
        {
        case BREAKPOINT_ENABLED:
            break;
        case BREAKPOINT_DISABLED:
            fprintf( command_output, "disable %d\n", i + 1 );
            break;
        case BREAKPOINT_DELETED:
            fprintf( command_output, "delete %d\n", i + 1 );
            break;
        default:
            assert( !"Wrong breakpoint status." );
        }
    }
    fflush( command_output );
}

#endif

static void debug_start_child( int argc, const char * * argv )
{
#if NT
    char buf[ 80 ];
    HANDLE pipe1[ 2 ];
    HANDLE pipe2[ 2 ];
    string self[ 1 ];
    string command_line[ 1 ];
    SECURITY_ATTRIBUTES sa = { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE };
    PROCESS_INFORMATION pi = { NULL, NULL, 0, 0 };
    STARTUPINFO si = { sizeof( STARTUPINFO ), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0 };
    assert( debug_state == DEBUG_NO_CHILD );
    if ( ! CreatePipe( &pipe1[ 0 ], &pipe1[ 1 ], &sa, 0 ) )
    {
        printf("internal error: CreatePipe:1: 0x$08x\n", GetLastError());
        return;
    }
    if ( ! CreatePipe( &pipe2[ 0 ], &pipe2[ 1 ], &sa, 0 ) )
    {
        printf("internal error: CreatePipe:2: 0x$08x\n", GetLastError());
        CloseHandle( pipe1[ 0 ] );
        CloseHandle( pipe1[ 1 ] );
        return;
    }
    string_new( self );
    if ( ! get_module_filename( self ) )
    {
        printf("internal error\n");
        CloseHandle( pipe1[ 0 ] );
        CloseHandle( pipe1[ 1 ] );
        CloseHandle( pipe2[ 0 ] );
        CloseHandle( pipe2[ 1 ] );
        return;
    }
    string_copy( command_line, "b2 " );
    /* Pass the handles as the first and second arguments. */
    string_append( command_line, debugger_opt );
    sprintf( buf, "%p", pipe1[ 0 ] );
    string_append( command_line, buf );
    string_push_back( command_line, ' ' );
    string_append( command_line, debugger_opt );
    sprintf( buf, "%p", pipe2[ 1 ] );
    string_append( command_line, buf );
    /* Pass the rest of the command line. */
	{
        int i;
        for ( i = 1; i < argc; ++i )
        {
            string_push_back( command_line, ' ' );
            string_append( command_line, argv[ i ] );
        }
    }
    SetHandleInformation( pipe1[ 1 ], HANDLE_FLAG_INHERIT, 0 );
    SetHandleInformation( pipe2[ 0 ], HANDLE_FLAG_INHERIT, 0 );
    if ( ! CreateProcess(
        self->value,
        command_line->value,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
        ) )
    {
        printf("internal error\n");
        CloseHandle( pipe1[ 0 ] );
        CloseHandle( pipe1[ 1 ] );
        CloseHandle( pipe2[ 0 ] );
        CloseHandle( pipe2[ 1 ] );
        string_free( self );
        string_free( command_line );
        return;
    }
    child_pid = pi.dwProcessId;
    child_handle = pi.hProcess;
    CloseHandle( pi.hThread );
    CloseHandle( pipe1[ 0 ] );
    CloseHandle( pipe2[ 1 ] );
    string_free( self );
    string_free( command_line );

    debug_state = DEBUG_RUN;

    init_parent_handles( pipe1[ 1 ], pipe2[ 0 ] );
    debug_parent_wait( 1 );
    debug_parent_copy_breakpoints();
    fprintf( command_output, "continue\n" );
    fflush( command_output );
#else
    int pipe1[2];
    int pipe2[2];
    int write_fd;
    int read_fd;
    int pid;
    int i;
    assert( debug_state == DEBUG_NO_CHILD );
    if (pipe(pipe1) == -1)
    {
        printf("internal error: pipe:1: %s\n", strerror(errno));
        return;
    }
    if (pipe(pipe2) == -1)
    {
        close( pipe1[ 0 ] );
        close( pipe1[ 1 ] );
        printf("internal error: pipe:2: %s\n", strerror(errno));
        return;
    }

    pid = fork();
    if ( pid == -1 )
    {
        /* error */
        close( pipe1[ 0 ] );
        close( pipe1[ 1 ] );
        close( pipe2[ 0 ] );
        close( pipe2[ 1 ] );
        printf("internal error: fork: %s\n", strerror(errno));
        return;
    }
    else if ( pid == 0 )
    {
        /* child */
        extern const char * saved_argv0;
        read_fd = pipe1[ 0 ];
        write_fd = pipe2[ 1 ];
        close( pipe2[ 0 ] );
        close( pipe1[ 1 ] );
        command_array = child_commands;
        argv[ 0 ] = executable_path( saved_argv0 );
        debug_child_data.argc = argc;
        debug_child_data.argv = argv;
        command_input = fdopen( read_fd, "r" );
        command_output = fdopen( write_fd, "w" );
        longjmp( debug_child_data.jmp, 1 );
    }
    else
    {
        /* parent */
        read_fd = pipe2[ 0 ];
        write_fd = pipe1[ 1 ];
        close( pipe1[ 0 ] );
        close( pipe2[ 1 ] );
        command_output = fdopen( write_fd, "w" );
        command_child = fdopen( read_fd, "r" );
        child_pid = pid;
    }
    debug_state = DEBUG_RUN;
#endif
}

static void debug_parent_run( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_RUN )
    {
        fprintf( command_output, "kill\n" );
        fflush( command_output );
        debug_parent_wait( 1 );
    }
    debug_parent_run_print( argc, argv );
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        printf( "=thread-created,id=\"1\",group-id=\"i1\"\n" );
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
    }
    debug_start_child( argc, argv );
    debug_parent_wait( 1 );
}

static int debug_parent_forward_nowait( int argc, const char * * argv, int print_message, int require_child )
{
    int i;
    if ( debug_state == DEBUG_NO_CHILD )
    {
        if ( require_child )
            printf( "The program is not being run.\n" );
        return 1;
    }
    fputs( argv[ 0 ], command_output );
    for( i = 1; i < argc; ++i )
    {
        fputc( ' ', command_output );
        fputs( argv[ i ], command_output );
    }
    fputc( '\n', command_output );
    fflush( command_output );
    return 0;
}

/* FIXME: This function should be eliminated when I finish all stdout to the parent. */
static void debug_parent_forward( int argc, const char * * argv, int print_message, int require_child )
{
    if ( debug_parent_forward_nowait( argc, argv, print_message, require_child ) != 0 )
    {
        return;
    }
    debug_parent_wait( print_message );
}

static void debug_parent_continue( int argc, const char * * argv )
{
    if ( argc > 1 )
    {
        debug_error( "Too many arguments to continue." );
        return;
    }
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
    }
    debug_parent_forward( 1, argv, 1, 1 );
}

static void debug_parent_kill( int argc, const char * * argv )
{
    if ( argc > 1 )
    {
        debug_error( "Too many arguments to kill." );
        return;
    }
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
        fflush( stdout );
    }
    debug_parent_forward( 1, argv, 0, 1 );
}

static void debug_parent_step( int argc, const char * * argv )
{
    if ( argc > 1 )
    {
        debug_error( "Too many arguments to step." );
        return;
    }
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
    }
    debug_parent_forward( 1, argv, 1, 1 );
}

static void debug_parent_next( int argc, const char * * argv )
{
    if ( argc > 1 )
    {
        debug_error( "Too many arguments to next." );
        return;
    }
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
    }
    debug_parent_forward( 1, argv, 1, 1 );
}

static void debug_parent_finish( int argc, const char * * argv )
{
    if ( argc > 1 )
    {
        debug_error( "Too many arguments to finish." );
        return;
    }
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
    }
    debug_parent_forward( 1, argv, 1, 1 );
}

static void debug_parent_break( int argc, const char * * argv )
{
    int id;
    if ( argc < 2 )
    {
        debug_error( "Missing argument to break." );
        return;
    }
    else if ( argc > 2 )
    {
        debug_error( "Too many arguments to break." );
        return;
    }
    id = debug_add_breakpoint( argv[ 1 ] );
    debug_parent_forward_nowait( argc, argv, 1, 0 );
    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "Breakpoint %d set at %s\n", id, argv[ 1 ] );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }
    else
    {
        assert( !"wrong value of debug_interface." );
    }
}

int check_breakpoint_fn_args( int argc, const char * * argv )
{
    if ( argc < 2 )
    {
        debug_error( "Missing argument to %s.", argv[ 0 ] );
        return 0;
    }
    else if ( argc > 2 )
    {
        debug_error( "Too many arguments to %s.", argv[ 0 ] );
        return 0;
    }
    else
    {
        char * end;
        long x = strtol( argv[ 1 ], &end, 10 );
        if ( *end )
        {
            debug_error( "Invalid breakpoint number %s.", argv[ 1 ] );
            return 0;
        }
        if ( x < 1 || x > num_breakpoints || breakpoints[ x - 1 ].status == BREAKPOINT_DELETED )
        {
            debug_error( "Unknown breakpoint %s.", argv[ 1 ] );
            return 0;
        }
    }
    return 1;
}

static void debug_parent_disable( int argc, const char * * argv )
{
    if ( ! check_breakpoint_fn_args( argc, argv ) )
    {
        return;
    }
    debug_child_disable( argc, argv );
    debug_parent_forward_nowait( 2, argv, 1, 0 );
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }
}

static void debug_parent_enable( int argc, const char * * argv )
{
    if ( ! check_breakpoint_fn_args( argc, argv ) )
    {
        return;
    }
    debug_child_enable( argc, argv );
    debug_parent_forward_nowait( 2, argv, 1, 0 );
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }
}

static void debug_parent_delete( int argc, const char * * argv )
{
    if ( ! check_breakpoint_fn_args( argc, argv ) )
    {
        return;
    }
    debug_child_delete( argc, argv );
    debug_parent_forward_nowait( 2, argv, 1, 0 );
    if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }
}

static void debug_parent_clear( int argc, const char * * argv )
{
    char buf[ 16 ];
    const char * new_args[ 2 ];
    int id;
    if ( argc < 2 )
    {
        debug_error( "Missing argument to clear." );
        return;
    }
    else if ( argc > 2 )
    {
        debug_error( "Too many arguments to clear." );
        return;
    }
    id = get_breakpoint_by_name( argv[ 1 ] );
    if ( id == 0 )
    {
        debug_error( "No breakpoint at %s.", argv[ 1 ] );
        return;
    }

    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        printf( "Deleted breakpoint %d\n", id );
    }
    
    sprintf( buf, "%d", id );
    new_args[ 0 ] = "delete";
    new_args[ 1 ] = buf;
    debug_parent_delete( 2, new_args );
}

static void debug_parent_print( int argc, const char * * argv )
{
    LIST * result;
    if ( debug_parent_forward_nowait( argc, argv, 1, 1 ) != 0 )
    {
        return;
    }
    result = debug_list_read( command_child );

    if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
    {
        list_print( result );
        printf( "\n" );
    }
    else if ( debug_interface == DEBUG_INTERFACE_MI )
    {
        printf( "~\"$1 = " );
        list_print( result );
        printf( "\"\n~\"\\n\"\n" );
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }

    list_free( result );
}

static void debug_parent_backtrace( int argc, const char * * argv )
{
    const char * new_args[ 3 ];
    OBJECT * depth_str;
    int depth;
    int i;
    FRAME_INFO frame;
    
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_error( "The program is not being run." );
        return;
    }

    new_args[ 0 ] = "info";
    new_args[ 1 ] = "frame";

    fprintf( command_output, "info depth\n" );
    fflush( command_output );
    depth_str = debug_object_read( command_child );
    depth = atoi( object_str( depth_str ) );
    object_free( depth_str );

    for ( i = 0; i < depth; ++i )
    {
        char buf[ 16 ];
        sprintf( buf, "%d", i );
        new_args[ 2 ] = buf;
        debug_parent_forward_nowait( 3, new_args, 0, 0 );
        debug_frame_read( command_child, &frame );
        printf( "#%d  in ", i );
        debug_print_frame_info( &frame );
        printf( "\n" );
    }
    fflush( stdout );
}

static void debug_parent_quit( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_RUN )
    {
        fprintf( command_output, "kill\n" );
        fflush( command_output );
        debug_parent_wait( 0 );
    }
    exit( 0 );
}

static const char * const help_text[][2] =
{
    {
        "run", 
        "run <args>\n"
        "Creates a new b2 child process passing <args> on the command line."
        "  Terminates\nthe current child (if any).\n"
    },
    {
        "continue",
        "continue\nContinue debugging\n"
    },
    {
        "step",
        "step\nContinue to the next statement\n"
    },
    {
        "next",
        "next\nContinue to the next line in the current frame\n"
    },
    {
        "finish",
        "finish\nContinue to the end of the current frame\n"
    },
    {
        "break",
        "break <location>\n"
        "Sets a breakpoint at <location>.  <location> can be either a the name of a\nfunction or <filename>:<lineno>\n"
    },
    {
        "disable",
        "disable <breakpoint>\nDisable a breakpoint\n"
    },
    {
        "enable",
        "enable <breakpoint>\nEnable a breakpoint\n"
    },
    {
        "delete",
        "delete <breakpoint>\nDelete a breakpoint\n"
    },
    {
        "clear",
        "clear <location>\nDelete the breakpoint at <location>\n"
    },
    {
        "print",
        "print <expression>\nDisplay the value of <expression>\n"
    },
    {
        "backtrace",
        "backtrace\nDisplay the call stack\n"
    },
    {
        "kill",
        "kill\nTerminate the child\n"
    },
    {
        "quit",
        "quit\nExit the debugger\n"
    },
    {
        "help",
        "help\nhelp <command>\nShow help for debugger commands.\n"
    },
    { 0, 0 }
};

static void debug_parent_help( int argc, const char * * argv )
{
    if ( argc == 1 )
    {
        printf(
            "run       - Start debugging\n"
            "continue  - Continue debugging\n"
            "step      - Continue to the next statement\n"
            "next      - Continue to the next line in the current frame\n"
            "finish    - Continue to the end of the current frame\n"
            "break     - Set a breakpoint\n"
            "disable   - Disable a breakpoint\n"
            "enable    - Enable a breakpoint\n"
            "delete    - Delete a breakpoint\n"
            "clear     - Delete a breakpoint by location\n"
            );
        printf(
            "print     - Display an expression\n"
            "backtrace - Display the call stack\n"
            "kill      - Terminate the child\n"
            "quit      - Exit the debugger\n"
            "help      - Debugger help\n"
            );
    }
    else if ( argc == 2 )
    {
        int i;
        for ( i = 0; help_text[ i ][ 0 ]; ++i )
        {
            if ( strcmp( argv[ 1 ], help_text[ i ][ 0 ] ) == 0 )
            {
                printf( "%s", help_text[ i ][ 1 ] );
                return;
            }
        }
        printf( "No command named %s\n", argv[ 1 ] );
    }
}

static void debug_mi_break_insert( int argc, const char * * argv );
static void debug_mi_break_delete( int argc, const char * * argv );
static void debug_mi_break_disable( int argc, const char * * argv );
static void debug_mi_break_enable( int argc, const char * * argv );
static void debug_mi_break_info( int argc, const char * * argv );
static void debug_mi_break_list( int argc, const char * * argv );
static void debug_mi_inferior_tty_set( int argc, const char * * argv );
static void debug_mi_gdb_exit( int argc, const char * * argv );
static void debug_mi_gdb_set( int argc, const char * * argv );
static void debug_mi_gdb_show( int argc, const char * * argv );
static void debug_mi_not_implemented( int argc, const char * * argv );
static void debug_mi_file_list_exec_source_files( int argc, const char * * argv );
static void debug_mi_file_list_exec_source_file( int argc, const char * * argv );
static void debug_mi_thread_info( int argc, const char * * argv );
static void debug_mi_thread_select( int argc, const char * * argv );
static void debug_mi_stack_info_frame( int argc, const char * * argv );
static void debug_mi_stack_select_frame( int argc, const char * * argv );
static void debug_mi_stack_list_variables( int argc, const char * * argv );
static void debug_mi_stack_list_locals( int argc, const char * * argv );
static void debug_mi_stack_list_frames( int argc, const char * * argv );
static void debug_mi_list_target_features( int argc, const char * * argv );
static void debug_mi_exec_run( int argc, const char * * argv );
static void debug_mi_exec_continue( int argc, const char * * argv );
static void debug_mi_exec_step( int argc, const char * * argv );
static void debug_mi_exec_next( int argc, const char * * argv );
static void debug_mi_exec_finish( int argc, const char * * argv );
static void debug_mi_data_list_register_names( int argc, const char * * argv );
static void debug_mi_data_evaluate_expression( int argc, const char * * argv );
static void debug_mi_interpreter_exec( int argc, const char * * argv );

static struct command_elem parent_commands[] =
{
    { "run", &debug_parent_run },
    { "continue", &debug_parent_continue },
    { "kill", &debug_parent_kill },
    { "step", &debug_parent_step },
    { "next", &debug_parent_next },
    { "finish", &debug_parent_finish },
    { "break", &debug_parent_break },
    { "disable", &debug_parent_disable },
    { "enable", &debug_parent_enable },
    { "delete", &debug_parent_delete },
    { "clear", &debug_parent_clear },
    { "print", &debug_parent_print },
    { "backtrace", &debug_parent_backtrace },
    { "quit", &debug_parent_quit },
    { "help", &debug_parent_help },
    { "-break-insert", &debug_mi_break_insert },
    { "-break-delete", &debug_mi_break_delete },
    { "-break-disable", &debug_mi_break_disable },
    { "-break-enable", &debug_mi_break_enable },
    { "-break-info", &debug_mi_break_info },
    { "-break-list", &debug_mi_break_list },
    { "-inferior-tty-set", &debug_mi_inferior_tty_set },
    { "-gdb-exit", &debug_mi_gdb_exit },
    { "-gdb-set", &debug_mi_gdb_set },
    { "-gdb-show", &debug_mi_gdb_show },
    { "-enable-pretty-printing", &debug_mi_not_implemented },
    { "-file-list-exec-source-files", &debug_mi_file_list_exec_source_files },
    { "-file-list-exec-source-file", &debug_mi_file_list_exec_source_file },
    { "-thread-info", &debug_mi_thread_info },
    { "-thread-select", &debug_mi_thread_select },
    { "-stack-info-frame", &debug_mi_stack_info_frame },
    { "-stack-select-frame", &debug_mi_stack_select_frame },
    { "-stack-list-variables", &debug_mi_stack_list_variables },
    { "-stack-list-locals", &debug_mi_stack_list_locals },
    { "-stack-list-frames", &debug_mi_stack_list_frames },
    { "-list-target-features", &debug_mi_list_target_features },
    { "-exec-run", &debug_mi_exec_run },
    { "-exec-continue", &debug_mi_exec_continue },
    { "-exec-step", &debug_mi_exec_step },
    { "-exec-next", &debug_mi_exec_next },
    { "-exec-finish", &debug_mi_exec_finish },
    { "-data-list-register-names", &debug_mi_data_list_register_names },
    { "-data-evaluate-expression", &debug_mi_data_evaluate_expression },
    { "-interpreter-exec", &debug_mi_interpreter_exec },
    { NULL, NULL }
};

static void debug_mi_format_token( void )
{
    if ( current_token != 0 )
    {
        printf( "%d", current_token );
    }
}

static void debug_mi_format_breakpoint( int id )
{
    struct breakpoint * ptr = &breakpoints[ id - 1 ];
    printf( "bkpt={" );
    printf( "number=\"%d\"", id );
    printf( ",type=\"breakpoint\"" );
    printf( ",disp=\"keep\"" ); /* FIXME: support temporary breakpoints. */
    printf( ",enabled=\"%s\"", ptr->status == BREAKPOINT_ENABLED ? "y" : "n" );
    /* addr */
    if ( ptr->line == -1 )
    {
        printf( ",func=\"%s\"", object_str( ptr->file ) );
    }
    else
    {
        printf( ",file=\"%s\"", object_str( ptr->file ) );
        printf( ",line=\"%d\"", ptr->line );
        printf( ",fullname=\"%s\"", object_str( ptr->file ) );
    }
    /* fullname */
    /* times */
    printf( "" );
    printf( "}" );
}

static int breakpoint_id_parse( const char * name )
{
    int id = atoi( name );
    if ( id > num_breakpoints || id < 1 || breakpoints[ id ].status == BREAKPOINT_DELETED )
        return -1;
    return id;
}

static void debug_mi_break_after( int argc, const char * * argv )
{
    int id;
    int count;
    --argc;
    ++argv;
    if ( argc > 0 && strcmp( argv[ 0 ], "--" ) == 0 )
    {
        ++argv;
        --argc;
    }
    if ( argc < 2 )
    {
        debug_mi_error( "not enough arguments for -break-after." );
        return;
    }
    else if ( argc > 2 )
    {
        debug_mi_error( "too many arguments for -break-after." );
        return;
    }
    id = atoi( argv[ 0 ] );
    count = atoi( argv[ 1 ] );
    /* FIXME: set ignore count */
}

static void debug_mi_break_insert( int argc, const char * * argv )
{
    const char * inner_argv[ 2 ];
    int temporary = 0; /* FIXME: not supported yet */
    int hardware = 0; /* unsupported */
    int force = 1; /* We don't have global debug information... */
    int disabled = 0;
    int tracepoint = 0; /* unsupported */
    int thread_id = 0;
    int ignore_count = 0;
    const char * condition; /* FIXME: not supported yet */
    const char * location;
    int id;
    for ( --argc, ++argv; argc; --argc, ++argv )
    {
        if ( strcmp( *argv, "-t" ) == 0 )
        {
            temporary = 1;
        }
        else if ( strcmp( *argv, "-h" ) == 0 )
        {
            hardware = 1;
        }
        else if ( strcmp( *argv, "-f" ) == 0 )
        {
            force = 1;
        }
        else if ( strcmp( *argv, "-d" ) == 0 )
        {
            disabled = 1;
        }
        else if ( strcmp( *argv, "-a" ) == 0 )
        {
            tracepoint = 1;
        }
        else if ( strcmp( *argv, "-c" ) == 0 )
        {
            if ( argc < 2 )
            {
                debug_mi_error( "Missing argument for -c." );
                return;
            }

            condition = argv[ 1 ];
            --argc;
            ++argv;
        }
        else if ( strcmp( *argv, "-i" ) == 0 )
        {
            if ( argc < 2 )
            {
                debug_mi_error( "Missing argument for -i." );
                return;
            }

            ignore_count = atoi( argv[ 1 ] );
            --argc;
            ++argv;
        }
        else if ( strcmp( *argv, "-p" ) == 0 )
        {
            if ( argc < 2 )
            {
                debug_mi_error( "Missing argument for -p." );
                return;
            }

            thread_id = atoi( argv[ 1 ] );
            --argc;
            ++argv;
        }
        else if ( strcmp( *argv, "--" ) == 0 )
        {
            --argc;
            ++argv;
            break;
        }
        else if ( **argv != '-' )
        {
            break;
        }
        else
        {
            debug_mi_error( "Unknown argument." );
            return;
        }
    }
    if ( argc > 1 )
    {
        debug_mi_error( "Too many arguments for -break-insert." );
        return;
    }

    if ( argc == 1 )
    {
        location = *argv;
    }
    else
    {
        debug_mi_error( "Not implemented: -break-insert with no location." );
        return;
    }
    inner_argv[ 0 ] = "break";
    inner_argv[ 1 ] = location;

    id = debug_add_breakpoint( location );
    debug_parent_forward_nowait( 2, inner_argv, 1, 0 );

    if ( disabled )
    {
        char buf[ 80 ];
        sprintf( buf, "%d", num_breakpoints );
        inner_argv[ 0 ] = "disable";
        inner_argv[ 1 ] = buf;
        debug_child_disable( 2, inner_argv );
        debug_parent_forward_nowait( 2, inner_argv, 1, 0 );
    }

    debug_mi_format_token();
    printf( "^done," );
    debug_mi_format_breakpoint( id );
    printf( "\n(gdb) \n" );
}

static void debug_mi_break_delete( int argc, const char * * argv )
{
    if ( argc < 2 )
    {
        debug_mi_error( "Not enough arguments for -break-delete" );
        return;
    }
    for ( --argc, ++argv; argc; --argc, ++argv )
    {
        const char * inner_argv[ 2 ];
        int id = breakpoint_id_parse( *argv );
        if ( id == -1 )
        {
            debug_mi_error( "Not a valid breakpoint" );
            return;
        }
        inner_argv[ 0 ] = "delete";
        inner_argv[ 1 ] = *argv;
        debug_parent_delete( 2, inner_argv );
    }
}

static void debug_mi_break_enable( int argc, const char * * argv )
{
    if ( argc < 2 )
    {
        debug_mi_error( "Not enough arguments for -break-enable" );
        return;
    }
    for ( --argc, ++argv; argc; --argc, ++argv )
    {
        const char * inner_argv[ 2 ];
        int id = breakpoint_id_parse( *argv );
        if ( id == -1 )
        {
            debug_mi_error( "Not a valid breakpoint" );
            return;
        }
        inner_argv[ 0 ] = "enable";
        inner_argv[ 1 ] = *argv;
        debug_parent_enable( 2, inner_argv );
    }
}

static void debug_mi_break_disable( int argc, const char * * argv )
{
    if ( argc < 2 )
    {
        debug_mi_error( "Not enough arguments for -break-disable" );
        return;
    }
    for ( --argc, ++argv; argc; --argc, ++argv )
    {
        const char * inner_argv[ 2 ];
        int id = breakpoint_id_parse( *argv );
        if ( id == -1 )
        {
            debug_mi_error( "Not a valid breakpoint" );
            return;
        }
        inner_argv[ 0 ] = "disable";
        inner_argv[ 1 ] = *argv;
        debug_parent_disable( 2, inner_argv );
    }
}

static void debug_mi_format_breakpoint_header_col( int width, int alignment, const char * col_name, const char * colhdr )
{
    printf( "{width=\"%d\",alignment=\"%d\",col_name=\"%s\",colhdr=\"%s\"}", width, alignment, col_name, colhdr );
}

static void debug_mi_format_breakpoint_hdr( void )
{
    printf( "hdr=[" );
    debug_mi_format_breakpoint_header_col( 7, -1, "number", "Num" );
    printf( "," );
    debug_mi_format_breakpoint_header_col( 14, -1, "type", "Type" );
    printf( "," );
    debug_mi_format_breakpoint_header_col( 4, -1, "disp", "Disp" );
    printf( "," );
    debug_mi_format_breakpoint_header_col( 3, -1, "enabled", "Enb" );
    printf( "," );
    debug_mi_format_breakpoint_header_col( 10, -1, "addr", "Address" );
    printf( "," );
    debug_mi_format_breakpoint_header_col( 40, 2, "what", "What" );
    printf( "]" );
}

static void debug_mi_break_info( int argc, const char * * argv )
{
    int id;
    --argc;
    ++argv;
    if ( strcmp( *argv, "--" ) == 0 )
    {
        --argc;
        ++argv;
    }
    if ( argc < 1 )
    {
        debug_mi_error( "Not enough arguments for -break-info" );
        return;
    }
    if ( argc > 1 )
    {
        debug_mi_error( "Too many arguments for -break-info" );
    }

    id = breakpoint_id_parse( *argv );
    if ( id == -1 )
    {
        debug_mi_error( "No such breakpoint." );
        return;
    }

    printf( "^done,BreakpointTable={"
        "nr_rows=\"%d\",nr_cols=\"6\",", 1 );
    debug_mi_format_breakpoint_hdr();
    printf( ",body=[" );
    debug_mi_format_breakpoint( id );
    printf( "]}" );
    printf("\n(gdb) \n");
}

static void debug_mi_break_list( int argc, const char * * argv )
{
    int number;
    int i;
    int first;
    if ( argc > 2 || ( argc == 2 && strcmp( argv[ 1 ], "--" ) ) )
    {
        debug_mi_error( "Too many arguments for -break-list" );
        return;
    }
    
    number = 0;
    for ( i = 0; i < num_breakpoints; ++i )
        if ( breakpoints[ i ].status != BREAKPOINT_DELETED )
            ++number;
    debug_mi_format_token();
    printf( "^done,BreakpointTable={"
        "nr_rows=\"%d\",nr_cols=\"6\",", number );
    debug_mi_format_breakpoint_hdr();
    printf( ",body=[" );
    first = 1;
    for ( i = 0; i < num_breakpoints; ++i )
        if ( breakpoints[ i ].status != BREAKPOINT_DELETED )
        {
            if ( first ) first = 0;
            else printf( "," );
            debug_mi_format_breakpoint( i + 1 );
        }
    printf( "]}" );
    printf("\n(gdb) \n");
}

static void debug_mi_inferior_tty_set( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^done\n(gdb) \n" );
}

static void debug_mi_gdb_exit( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_RUN )
    {
        fprintf( command_output, "kill\n" );
        fflush( command_output );
        debug_parent_wait( 0 );
    }
    debug_mi_format_token();
    printf( "^exit\n" );
    exit( EXIT_SUCCESS );
}

static void debug_mi_gdb_set( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^done\n(gdb) \n" );
}

static void debug_mi_gdb_show( int argc, const char * * argv )
{
    const char * value = "";
    /* FIXME: implement this for real */
    debug_mi_format_token();
    value = "(gdb) ";
    printf( "^done,value=\"%s\"\n(gdb) \n", value );
}

static void debug_mi_not_implemented( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^done\n(gdb) \n" );
}

void debug_mi_file_list_exec_source_files( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^done,files=[]\n(gdb) \n" );
}

static void debug_mi_file_list_exec_source_file( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^error,msg=\"Don't know how to handle this yet\"\n(gdb) \n" );
}

static void debug_mi_thread_info( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^done,threads=[]\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "info", "frame" };
        FRAME_INFO info;
        debug_parent_forward_nowait( 2, new_args, 0, 0 );
        debug_frame_read( command_child, &info );

        debug_mi_format_token();
        printf( "^done,threads=[{id=\"1\"," );
        debug_mi_print_frame_info( &info );
        debug_frame_info_free( &info );
        printf( "}],current-thread-id=\"1\"\n(gdb) \n" );
    }
}

static void debug_mi_thread_select( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        /* FIXME: better error handling*/
        debug_mi_format_token();
        printf( "^error,msg=\"Thread ID 1 not known\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "info", "frame" };
        FRAME_INFO info;
        debug_parent_forward_nowait( 2, new_args, 0, 0 );
        debug_frame_read( command_child, &info );

        debug_mi_format_token();
        printf( "^done,new-thread-id=\"1\"," );
        debug_mi_print_frame_info( &info );
        debug_frame_info_free( &info );
        printf( "\n(gdb) \n" );
    }
}

static void debug_mi_stack_select_frame( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[ 2 ];
        new_args[ 0 ] = "frame";
        new_args[ 1 ] = argv[ 1 ];
        debug_parent_forward_nowait( 2, new_args, 0, 0 );
        debug_mi_format_token();
        printf( "^done\n(gdb) \n" );
    }
}

static void debug_mi_stack_info_frame( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        FRAME_INFO info;
        fprintf( command_output, "info frame\n" );
        fflush( command_output );
        debug_frame_read( command_child, &info );
        debug_mi_format_token();
        printf( "^done," );
        debug_mi_print_frame_info( &info );
        debug_frame_info_free( &info );
        printf( "\n(gdb) \n" );
    }
}

static void debug_mi_stack_list_variables( int argc, const char * * argv )
{
    int print_values = 0;
#define DEBUG_PRINT_VARIABLES_NO_VALUES     1
#define DEBUG_PRINT_VARIABLES_ALL_VALUES    2
#define DEBUG_PRINT_VARIABLES_SIMPLE_VALUES 3
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
        return;
    }
    --argc;
    ++argv;
    for ( ; argc; --argc, ++argv )
    {
        if ( strcmp( *argv, "--thread" ) == 0 )
        {
            /* Only one thread. */
            --argc;
            ++argv;
        }
        else if ( strcmp( *argv, "--no-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_NO_VALUES;
        }
        else if ( strcmp( *argv, "--all-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_ALL_VALUES;
        }
        else if ( strcmp( *argv, "--simple-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_SIMPLE_VALUES;
        }
        else if ( strcmp( *argv, "--" ) == 0 )
        {
            --argc;
            ++argv;
            break;
        }
        else if ( argv[ 0 ][ 0 ] == '-' )
        {
            debug_mi_format_token();
            printf( "^error,msg=\"Unknown argument %s\"\n(gdb) \n", *argv );
            return;
        }
        else
        {
            break;
        }
    }
    if ( argc != 0 )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"Too many arguments for -stack-list-variables\"\n(gdb) \n" );
        return;
    }
    
    {
        LIST * vars;
        LISTITER iter, end;
        int first = 1;
        fprintf( command_output, "info locals\n" );
        fflush( command_output );
        vars = debug_list_read( command_child );
        debug_parent_wait( 0 );
        debug_mi_format_token();
        printf( "^done,variables=[" );
        for ( iter = list_begin( vars ), end = list_end( vars ); iter != end; iter = list_next( iter ) )
        {
            OBJECT * varname = list_item( iter );
            string varbuf[1];
            const char * new_args[2];
            if ( first )
            {
                first = 0;
            }
            else
            {
                printf( "," );
            }
            printf( "{name=\"%s\",value=\"", object_str( varname ) );
            fflush( stdout );
            string_new( varbuf );
            string_append( varbuf, "$(" );
            string_append( varbuf, object_str( varname ) );
            string_append( varbuf, ")" );
            new_args[ 0 ] = "print";
            new_args[ 1 ] = varbuf->value;
            debug_parent_forward( 2, new_args, 0, 0 );
            string_free( varbuf );
            printf( "\"}" );
        }
        printf( "]\n(gdb) \n" );
        fflush( stdout );
        list_free( vars );
    }
}

static void debug_mi_stack_list_locals( int argc, const char * * argv )
{
    int print_values = 0;
#define DEBUG_PRINT_VARIABLES_NO_VALUES     1
#define DEBUG_PRINT_VARIABLES_ALL_VALUES    2
#define DEBUG_PRINT_VARIABLES_SIMPLE_VALUES 3
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
        return;
    }
    --argc;
    ++argv;
    for ( ; argc; --argc, ++argv )
    {
        if ( strcmp( *argv, "--thread" ) == 0 )
        {
            /* Only one thread. */
            --argc;
            ++argv;
            if ( argc == 0 )
            {
                debug_mi_format_token();
                printf( "^error,msg=\"Argument required for --thread.\"" );
                return;
            }
        }
        else if ( strcmp( *argv, "--no-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_NO_VALUES;
        }
        else if ( strcmp( *argv, "--all-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_ALL_VALUES;
        }
        else if ( strcmp( *argv, "--simple-values" ) == 0 )
        {
            print_values = DEBUG_PRINT_VARIABLES_SIMPLE_VALUES;
        }
        else if ( strcmp( *argv, "--" ) == 0 )
        {
            --argc;
            ++argv;
            break;
        }
        else if ( argv[ 0 ][ 0 ] == '-' )
        {
            debug_mi_format_token();
            printf( "^error,msg=\"Unknown argument %s\"\n(gdb) \n", *argv );
            return;
        }
        else
        {
            break;
        }
    }
    if ( argc != 0 )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"Too many arguments for -stack-list-variables\"\n(gdb) \n" );
        return;
    }
    
    {
        LIST * vars;
        LISTITER iter, end;
        int first = 1;
        fprintf( command_output, "info locals\n" );
        fflush( command_output );
        vars = debug_list_read( command_child );
        debug_parent_wait( 0 );
        debug_mi_format_token();
        printf( "^done,locals=[" );
        for ( iter = list_begin( vars ), end = list_end( vars ); iter != end; iter = list_next( iter ) )
        {
            OBJECT * varname = list_item( iter );
            string varbuf[1];
            const char * new_args[2];
            if ( first )
            {
                first = 0;
            }
            else
            {
                printf( "," );
            }
            printf( "{name=\"%s\",type=\"list\",value=\"", object_str( varname ) );
            fflush( stdout );
            string_new( varbuf );
            string_append( varbuf, "$(" );
            string_append( varbuf, object_str( varname ) );
            string_append( varbuf, ")" );
            new_args[ 0 ] = "print";
            new_args[ 1 ] = varbuf->value;
            debug_parent_forward( 2, new_args, 0, 0 );
            string_free( varbuf );
            printf( "\"}" );
        }
        printf( "]\n(gdb) \n" );
        fflush( stdout );
        list_free( vars );
    }
}

static void debug_mi_stack_list_frames( int argc, const char * * argv )
{
    const char * new_args[ 3 ];
    int depth;
    int i;
    
    if ( debug_state == DEBUG_NO_CHILD )
    {
        debug_mi_format_token();
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
        return;
    }

    new_args[ 0 ] = "info";
    new_args[ 1 ] = "frame";

    fprintf( command_output, "info depth\n" );
    fflush( command_output );
    depth = debug_int_read( command_child );

    debug_mi_format_token();
    printf( "^done,stack=[" );
    for ( i = 0; i < depth; ++i )
    {
        FRAME_INFO frame;
        fprintf( command_output, "info frame %d\n", i );
        fflush( command_output );
        if ( i != 0 )
        {
            printf( "," );
        }
        debug_frame_read( command_child, &frame );
        debug_mi_print_frame_info( &frame );
    }
    printf( "]\n(gdb) \n" );
    fflush( stdout );
}

static void debug_mi_list_target_features( int argc, const char * * argv )
{
    /* FIXME: implement this for real */
    debug_mi_format_token();
    printf( "^done,features=[\"async\"]\n(gdb) \n" );
}

static void debug_mi_exec_run( int argc, const char * * argv )
{
    printf( "=thread-created,id=\"1\",group-id=\"i1\"\n" );
    debug_mi_format_token();
    printf( "^running\n(gdb) \n" );
    fflush( stdout );
    debug_start_child( argc, argv );
    debug_parent_wait( 1 );
}

static void debug_mi_exec_continue( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "continue" };
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
        debug_parent_forward( 1, new_args, 1, 0 );
    }
}

static void debug_mi_exec_step( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "step" };
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
        debug_parent_forward( 1, new_args, 1, 0 );
    }
}

static void debug_mi_exec_next( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "next" };
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
        debug_parent_forward( 1, new_args, 1, 0 );
    }
}

static void debug_mi_exec_finish( int argc, const char * * argv )
{
    if ( debug_state == DEBUG_NO_CHILD )
    {
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[] = { "finish" };
        debug_mi_format_token();
        printf( "^running\n(gdb) \n" );
        fflush( stdout );
        debug_parent_forward( 1, new_args, 1, 0 );
    }
}

static void debug_mi_data_list_register_names( int argc, const char * * argv )
{
    debug_mi_format_token();
    printf( "^done,register-names=[]\n(gdb) \n" );
}

static void debug_mi_data_evaluate_expression( int argc, const char * * argv )
{
    if ( argc < 2 )
    {
        printf( "^error,msg=\"Not enough arguments for -data-evaluate-expression\"\n(gdb) \n" );
    }
    if ( debug_state == DEBUG_NO_CHILD )
    {
        printf( "^error,msg=\"No child\"\n(gdb) \n" );
    }
    else
    {
        const char * new_args[ 2 ];
        debug_mi_format_token();
        printf( "^done,value=\"" );
        fflush( stdout );
        new_args[ 0 ] = "print";
        new_args[ 1 ] = argv[ 1 ];
        debug_parent_forward( 2, new_args, 1, 0 );
        printf( "\"\n(gdb) \n" );
    }
}

static int process_command( char * command );

static void debug_mi_interpreter_exec( int argc, const char * * argv )
{
    if ( argc < 3 )
    {
        debug_mi_error( "Not enough arguments for -interpreter-exec" );
    }
    process_command( (char *)argv[ 2 ] );
}

/* The debugger's main loop. */
int debugger( void )
{
    command_array = parent_commands;
    command_input = stdin;
    if ( debug_interface == DEBUG_INTERFACE_MI )
        printf( "=thread-group-added,id=\"i1\"\n(gdb) \n" );
    while ( 1 )
    {
        if ( debug_interface == DEBUG_INTERFACE_CONSOLE )
            printf("(b2db) ");
        fflush( stdout );
        read_command();
    }
    return 0;
}


/* Runs the matching command in the current command_array. */
static int run_command( int argc, const char * * argv )
{
    struct command_elem * command;
    const char * command_name;
    if ( argc == 0 )
    {
        return 1;
    }
    command_name = argv[ 0 ];
    /* Skip the GDB/MI token when choosing the command to run. */
    while( isdigit( *command_name ) ) ++command_name;
    current_token = atoi( argv[ 0 ] );
    for( command = command_array; command->key; ++command )
    {
        if ( strcmp( command->key, command_name ) == 0 )
        {
            ( *command->command )( argc, argv );
            return 1;
        }
    }
    debug_error( "Unknown command: %s", command_name );
    return 0;
}

/* Parses a single command into whitespace separated tokens, and runs it. */
static int process_command( char * line )
{
    int result;
    size_t capacity = 8;
    const char * * buffer = malloc( capacity * sizeof( const char * ) );
    const char * * current = buffer;
    char * iter = line;
    const char * saved = iter;
    *current = iter;
    for ( ; ; )
    {
        /* skip spaces */
        while ( *iter && isspace( *iter ) )
        {
            ++iter;
        }
        if ( ! *iter )
        {
            break;
        }
        /* Find the next token */
        saved = iter;
        if ( *iter == '\"' )
        {
            saved = ++iter;
            /* FIXME: handle escaping */
            while ( *iter && *iter != '\"' )
            {
                ++iter;
            }
        }
        else
        {
            while ( *iter && ! isspace( *iter ) )
            {
                ++iter;
            }
        }
        /* resize the buffer if necessary */
        if ( current == buffer + capacity )
        {
            buffer = realloc( (void *)buffer, capacity * 2 * sizeof( const char * ) );
            current = buffer + capacity;
        }
        /* append the token to the buffer */
        *current++ = saved;
        /* null terminate the token */
        if ( *iter )
        {
            *iter++ = '\0';
        }
    }
    result = run_command( current - buffer, buffer );
    free( (void *)buffer );
    return result;
}

static int read_command( void )
{
    int result;
    int ch;
    string line[ 1 ];
    string_new( line );
    /* HACK: force line to be on the heap. */
    string_reserve( line, 64 );
    while( ( ch = fgetc( command_input ) )  != EOF )
    {
        if ( ch == '\n' )
        {
            break;
        }
        else
        {
            string_push_back( line, (char)ch );
        }
    }
    result = process_command( line->value );
    string_free( line );
    return result;
}

static void debug_listen( void )
{
    debug_state = DEBUG_STOPPED;
    while ( debug_state == DEBUG_STOPPED )
    {
        if ( feof( command_input ) )
            exit( 1 );
        fflush(stdout);
        fflush( command_output );
        read_command();
    }
    debug_selected_frame_number = 0;
}

struct debug_child_data_t debug_child_data;
const char debugger_opt[] = "--b2db-internal-debug-handle=";
int debug_interface;
