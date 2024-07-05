/*
 * syslog-client.c - syslog client implementation for windows
 *
 * Created by Alexander Yaworsky
 *
 * THIS SOFTWARE IS NOT COPYRIGHTED
 *
 * This source code is offered for use in the public domain. You may
 * use, modify or distribute it freely.
 *
 * This code is distributed in the hope that it will be useful but
 * WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 * DISCLAIMED. This includes but is not limited to warranties of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/* define SYSLOG_CONF_DIR where syslog.host should be
 */

#ifndef SYSLOG_CONF_DIR
static const char *syslog_conf_dir = ".";
#else
static const char *syslog_conf_dir = SYSLOG_CONF_DIR;
#endif

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <WinBase.h>
#include "syslog.h"

#define SYSLOG_DGRAM_SIZE 8192
#define MAX_IDENT_LENGTH 128
#define MAX_APP_NAME_LENGTH 48

//static char UTF8_BOM[] = {0xEF, 0xBB, 0xBF, '\0', };
static char UTF8_BOM[] = {'\0'};

static BOOL initialized = FALSE;
static int log_mask = 0xFF;
static char syslog_ident[MAX_IDENT_LENGTH];
static int syslog_facility;
static char str_pid[ 40 ];
static SOCKADDR_IN sa_logger;
static SOCKET sock;
static char local_hostname[ MAX_COMPUTERNAME_LENGTH + 1 ];
static int datagramm_size;

static int version = 1;
static char app_name[MAX_APP_NAME_LENGTH];
static volatile LONG mid = 0;

/******************************************************************************
 * set_syslog_conf_dir
 *
 * maybe this function will be useful...
 */
const char* set_syslog_conf_dir( const char* dir )
{
    const char *ret = syslog_conf_dir;
    syslog_conf_dir = dir;
    return ret;
}

/******************************************************************************
 * init_logger_addr
 *
 * Read configuration file syslog.host. This file should contain host address
 * and, optionally, port. Initialize sa_logger. If the configuration file does
 * not exist, use localhost:514.
 * Returns: 0 - ok, -1 - error.
 */
static void init_logger_addr()
{
    char pathname[ FILENAME_MAX ];
    char *p;
    FILE *fd;
    char host[256];
    struct hostent * phe;

	/*
	UTF8_BOM[0] = (char)0xEF;
	UTF8_BOM[1] = (char)0xBB;
	UTF8_BOM[2] = (char)0xBF;
	UTF8_BOM[3] = '\0';
	*/

    memset( &sa_logger, 0, sizeof(SOCKADDR_IN) );
    sa_logger.sin_family = AF_INET;

    if( '\\' == syslog_conf_dir[0] || '/' == syslog_conf_dir[0] || ':' == syslog_conf_dir[1] )
    {
        /* absolute path */
        strcpy( pathname, syslog_conf_dir );
    }
    else
    {
        /* relative path */
        char *q;

        strcpy( pathname, __argv[0] );
        p = strrchr( pathname, '\\' ) + 1;
        q = strrchr( pathname, '/' ) + 1;
        if( p < q )
            *q = 0;
        else if( p > q )
            *p = 0;
        else
            pathname[0] = 0;
        strcat( pathname, syslog_conf_dir );
    }
    p = &pathname[ strlen( pathname ) - 1 ];
    if( '\\' != *p && '/' != *p )
    {
        p++; *p = '/';
    }
    strcpy( ++p, "syslog.host" );

    /* read destination host name */
    fd = fopen( pathname, "r" );
    if( !fd )
        goto use_default;

    if( NULL == fgets( host, sizeof(host), fd ) )
        host[0] = 0;
    else
    {
        p = strchr( host, '\n' );
        if( p )
            *p = 0;
        p = strchr( host, '\r' );
        if( p )
            *p = 0;
    }
    fclose( fd );

    p = strchr( host, ':' );
    if( p )
        *p++ = 0;

    phe = gethostbyname( host );
    if( !phe )
        goto use_default;

    memcpy( &sa_logger.sin_addr.s_addr, phe->h_addr, phe->h_length );

    if( p )
        sa_logger.sin_port = htons( (unsigned short) strtoul( p, NULL, 0 ) );
    else
        sa_logger.sin_port = htons( SYSLOG_PORT );
    return;

use_default:
    sa_logger.sin_addr.S_un.S_addr = htonl( 0x7F000001 );
    sa_logger.sin_port = htons( SYSLOG_PORT );
}

void set_syslog_host( const char* host, u_short port ) {
	struct hostent *phe = gethostbyname( host );
	if( !phe ) {
		return;
	}

	memcpy( &sa_logger.sin_addr.s_addr, phe->h_addr, phe->h_length );

	sa_logger.sin_port = htons( port );
}

/******************************************************************************
 * closelog
 *
 * Close desriptor used to write to system logger.
 */
void closelog(void)
{
    if( !initialized )
        return;
    closesocket( sock );
    WSACleanup();
    initialized = FALSE;
}

/******************************************************************************
 * openlog
 *
 * Open connection to system logger.
 */
void openlog( const char* ident, int option, int facility )
{
    BOOL failed = TRUE, wsa_initialized = FALSE;
    WSADATA wsd;
    SOCKADDR_IN sa_local;
    DWORD n;
	char exeFile[1024];
    int size;

    if( initialized )
        return;

    syslog_facility = facility? facility : LOG_USER;

    /* FIXME: should we reset logmask? */

    if( option & LOG_PID )
        _snprintf( str_pid, sizeof(str_pid), "%lu", GetCurrentProcessId() );
    else
        str_pid[0] = 0;

    /* FIXME: handle other options */

    n = sizeof(local_hostname);
    if( !GetComputerName( local_hostname, &n ) )
        goto done;

	n = GetModuleFileName(NULL, exeFile, 1024);
	if (n > 0) {
		while (--n) {
			if (exeFile[n] == '\\' ) {
				strncpy(app_name, &exeFile[n+1], MAX_APP_NAME_LENGTH);			
				break;
			}
		}
	}

    sock = INVALID_SOCKET;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsd ) )
        goto done;
    wsa_initialized = TRUE;

    init_logger_addr();

    for( n = 0;; n++ )
    {
        sock = socket( AF_INET, SOCK_DGRAM, 0 );
        if( INVALID_SOCKET == sock )
            goto done;

        memset( &sa_local, 0, sizeof(SOCKADDR_IN) );
        sa_local.sin_family = AF_INET;
        if( bind( sock, (SOCKADDR*) &sa_local, sizeof(SOCKADDR_IN) ) == 0 )
            break;
        closesocket( sock );
        sock = INVALID_SOCKET;
        if( n == 100 )
            goto done;
        Sleep(0);
    }

    /* get size of datagramm */
    size = sizeof(datagramm_size);
    if( getsockopt( sock, SOL_SOCKET, SO_MAX_MSG_SIZE, (char*) &datagramm_size, &size ) )
        goto done;
    if( datagramm_size - strlen(local_hostname) - (ident? strlen(ident) : 0) < 64 )
        goto done;
    if( datagramm_size > SYSLOG_DGRAM_SIZE)
        datagramm_size = SYSLOG_DGRAM_SIZE;

    if( atexit( closelog ) )
        goto done;

	strncpy(syslog_ident, ident, MAX_IDENT_LENGTH);
    syslog_facility = facility;
    failed = FALSE;

done:
    if( failed )
    {
        if( sock != INVALID_SOCKET ) closesocket( sock );
        if( wsa_initialized ) WSACleanup();
    }
    initialized = !failed;
}

/******************************************************************************
 * setlogmask
 *
 * Set the log mask level.
 */
int setlogmask( int mask )
{
    int ret = log_mask;

    if( mask )
        log_mask = mask;
    return ret;
}

/******************************************************************************
 * syslog
 *
 * Generate a log message using FMT string and option arguments.
 */
void syslog( int pri, const char* fmt, ... )
{
    va_list ap;

    va_start( ap, fmt );
    vsyslog( pri, fmt, ap );
    va_end( ap );
}

/******************************************************************************
 * vsyslog
 *
 * Generate a log message using FMT and using arguments pointed to by AP.
 */
void vsyslog( int pri, char* fmt, va_list ap )
{
    static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    SYSTEMTIME stm;
    int len;
    char *p;
	char datagramm[SYSLOG_DGRAM_SIZE];
	LONG current_mid = 0;
	char timestamp[128];

    if( !(LOG_MASK( LOG_PRI( pri )) & log_mask) )
        return;

    openlog( NULL, 0, pri & LOG_FACMASK );
    if( !initialized )
        return;

	if( !(pri & LOG_FACMASK) ) {
        pri |= syslog_facility;
	}

	current_mid = InterlockedIncrement(&mid);
	
    GetLocalTime( &stm );
	sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d.%dZ", stm.wYear, stm.wMonth, stm.wDay, stm.wHour, 
		stm.wMinute, stm.wSecond, stm.wMilliseconds);

	len = sprintf( datagramm, "<%d>%d %s %s %s %s %ld %s %s%s ",
		pri, version, timestamp, local_hostname, app_name, str_pid, current_mid, "", UTF8_BOM, syslog_ident);

    vsnprintf( datagramm + len, datagramm_size - len, fmt, ap );
    p = strchr( datagramm, '\n' );
    if( p )
        *p = 0;
    p = strchr( datagramm, '\r' );
    if( p )
        *p = 0;

	//printf("log: %s", datagramm);
    sendto( sock, datagramm, strlen(datagramm), 0, (SOCKADDR*) &sa_logger, sizeof(SOCKADDR_IN) );
}

/******************************************************************************
 * test
 */
#ifdef TEST

static HANDLE hRxEvent = NULL, hAckEvent = NULL;
static SOCKET rxsock = INVALID_SOCKET;
static char buffer[ sizeof(datagramm)+1 ];

static DWORD WINAPI listener( LPVOID param )
{
    for(;;)
    {
        int ret;

        WaitForSingleObject( hAckEvent, INFINITE );
        ret = recv( rxsock, buffer, sizeof(datagramm), 0 );
        if( ret <= 0 )
            break;
        buffer[ ret ] = 0;
        SetEvent( hRxEvent );
    }
    return 0;
}

static int transact( int pri, char* fmt, ... )
{
    va_list ap;
    DWORD r;

    va_start( ap, fmt );
    vsyslog( pri, fmt, ap );
    va_end( ap );
    r = WaitForSingleObject( hRxEvent, 2000 );
    if( WAIT_TIMEOUT == r )
    {
        fprintf( stderr, "timeout\n" );
        return -1;
    }
    if( WAIT_FAILED == r )
    {
        fprintf( stderr, "wait failed, error %lu\n", GetLastError() );
        return -1;
    }
    printf( "*** %s\n", buffer );
    SetEvent( hAckEvent );
    return 0;
}

int main( int argc, char* argv[] )
{
    int ret = 1;
    WSADATA wsd;
    SOCKADDR_IN sa_local;
    DWORD tid;
    HANDLE hThread = NULL;

    if( WSAStartup( MAKEWORD( 2, 2 ), &wsd ) )
    {
        fprintf( stderr, "WSAStartup() failed, error %d\n", WSAGetLastError() );
        return 1;
    }
    
    hRxEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( !hRxEvent )
    {
        fprintf( stderr, "CreateEvent() failed, error %lu\n", GetLastError() );
        goto done;
    }

    hAckEvent = CreateEvent( NULL, FALSE, TRUE, NULL );
    if( !hAckEvent )
    {
        fprintf( stderr, "CreateEvent() failed, error %lu\n", GetLastError() );
        goto done;
    }

    rxsock = socket( AF_INET, SOCK_DGRAM, 0 );
    if( INVALID_SOCKET == sock )
    {
        fprintf( stderr, "socket() failed, error %d\n", WSAGetLastError() );
        goto done;
    }

    memset( &sa_local, 0, sizeof(SOCKADDR_IN) );
    sa_local.sin_family = AF_INET;
    sa_local.sin_addr.S_un.S_addr = htonl( 0x7F000001 );
    sa_local.sin_port = htons( SYSLOG_PORT );
    if( bind( rxsock, (SOCKADDR*) &sa_local, sizeof(SOCKADDR_IN) ) )
    {
        fprintf( stderr, "bind() failed, error %d\n", WSAGetLastError() );
        goto done;
    }

    hThread = CreateThread( NULL, 0, listener, NULL, 0, &tid );
    if( !hThread )
    {
        fprintf( stderr, "CreateThread() failed, error %lu\n", GetLastError() );
        goto done;
    }

    openlog( "test_ident", 0, LOG_USER );
    if( !initialized )
    {
        fprintf( stderr, "openlog() failed\n" );
        goto done;
    }
    
    if( transact( LOG_DEBUG, "test message %d", 1 ) )
        goto done;

    setlogmask( LOG_MASK( LOG_EMERG ) );

    if( !transact( LOG_DEBUG, "test message %d", 2 ) )
        goto done;

    if( transact( LOG_EMERG, "test message %d", 3 ) )
        goto done;

    closelog();
    openlog( "test_ident", LOG_PID, LOG_USER );
    if( !initialized )
    {
        fprintf( stderr, "openlog() failed\n" );
        goto done;
    }

    setlogmask( LOG_MASK( LOG_DEBUG ) );

    if( transact( LOG_DEBUG, "test message %d with pid", 4 ) )
        goto done;

    if( transact( LOG_DEBUG, "long test message %d 1234567890 1234567890 1234567890 1234567890 1234567890", 5 ) )
        goto done;

    ret = 0;

done:
    if( rxsock != INVALID_SOCKET ) closesocket( rxsock );
    if( hAckEvent )
    {
        if( hThread )
        {
            SetEvent( hAckEvent );
            puts( "waiting for thread shutdown" );
            WaitForSingleObject( hThread, INFINITE );
        }
        CloseHandle( hAckEvent );
    }
    if( hThread ) CloseHandle( hThread );
    if( hRxEvent ) CloseHandle( hRxEvent );
    WSACleanup();
    return 0;
}

#endif /* TEST */
