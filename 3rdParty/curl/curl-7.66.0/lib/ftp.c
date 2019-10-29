/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2019, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include "curl_setup.h"

#ifndef CURL_DISABLE_FTP

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_UTSNAME_H
#include <sys/utsname.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef __VMS
#include <in.h>
#include <inet.h>
#endif

#if (defined(NETWARE) && defined(__NOVELL_LIBC__))
#undef in_addr_t
#define in_addr_t unsigned long
#endif

#include <curl/curl.h>
#include "urldata.h"
#include "sendf.h"
#include "if2ip.h"
#include "hostip.h"
#include "progress.h"
#include "transfer.h"
#include "escape.h"
#include "http.h" /* for HTTP proxy tunnel stuff */
#include "socks.h"
#include "ftp.h"
#include "fileinfo.h"
#include "ftplistparser.h"
#include "curl_range.h"
#include "curl_sec.h"
#include "strtoofft.h"
#include "strcase.h"
#include "vtls/vtls.h"
#include "connect.h"
#include "strerror.h"
#include "inet_ntop.h"
#include "inet_pton.h"
#include "select.h"
#include "parsedate.h" /* for the week day and month names */
#include "sockaddr.h" /* required for Curl_sockaddr_storage */
#include "multiif.h"
#include "url.h"
#include "strcase.h"
#include "speedcheck.h"
#include "warnless.h"
#include "http_proxy.h"
#include "non-ascii.h"
/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#ifdef CURL_DISABLE_VERBOSE_STRINGS
#define ftp_pasv_verbose(a,b,c,d)  Curl_nop_stmt
#endif

/* Local API functions */
#ifndef DEBUGBUILD
static void _state(struct connectdata *conn,
                   ftpstate newstate);
#define state(x,y) _state(x,y)
#else
static void _state(struct connectdata *conn,
                   ftpstate newstate,
                   int lineno);
#define state(x,y) _state(x,y,__LINE__)
#endif

static CURLcode ftp_sendquote(struct connectdata *conn,
                              struct curl_slist *quote);
static CURLcode ftp_quit(struct connectdata *conn);
static CURLcode ftp_parse_url_path(struct connectdata *conn);
static CURLcode ftp_regular_transfer(struct connectdata *conn, bool *done);
#ifndef CURL_DISABLE_VERBOSE_STRINGS
static void ftp_pasv_verbose(struct connectdata *conn,
                             Curl_addrinfo *ai,
                             char *newhost, /* ascii version */
                             int port);
#endif
static CURLcode ftp_state_prepare_transfer(struct connectdata *conn);
static CURLcode ftp_state_mdtm(struct connectdata *conn);
static CURLcode ftp_state_quote(struct connectdata *conn,
                                bool init, ftpstate instate);
static CURLcode ftp_nb_type(struct connectdata *conn,
                            bool ascii, ftpstate newstate);
static int ftp_need_type(struct connectdata *conn,
                         bool ascii);
static CURLcode ftp_do(struct connectdata *conn, bool *done);
static CURLcode ftp_done(struct connectdata *conn,
                         CURLcode, bool premature);
static CURLcode ftp_connect(struct connectdata *conn, bool *done);
static CURLcode ftp_disconnect(struct connectdata *conn, bool dead_connection);
static CURLcode ftp_do_more(struct connectdata *conn, int *completed);
static CURLcode ftp_multi_statemach(struct connectdata *conn, bool *done);
static int ftp_getsock(struct connectdata *conn, curl_socket_t *socks);
static int ftp_domore_getsock(struct connectdata *conn, curl_socket_t *socks);
static CURLcode ftp_doing(struct connectdata *conn,
                          bool *dophase_done);
static CURLcode ftp_setup_connection(struct connectdata * conn);

static CURLcode init_wc_data(struct connectdata *conn);
static CURLcode wc_statemach(struct connectdata *conn);

static void wc_data_dtor(void *ptr);

static CURLcode ftp_state_retr(struct connectdata *conn, curl_off_t filesize);

static CURLcode ftp_readresp(curl_socket_t sockfd,
                             struct pingpong *pp,
                             int *ftpcode,
                             size_t *size);
static CURLcode ftp_dophase_done(struct connectdata *conn,
                                 bool connected);

/* easy-to-use macro: */
#define PPSENDF(x,y,z)  result = Curl_pp_sendf(x,y,z); \
                        if(result)                     \
                          return result


/*
 * FTP protocol handler.
 */

const struct Curl_handler Curl_handler_ftp = {
  "FTP",                           /* scheme */
  ftp_setup_connection,            /* setup_connection */
  ftp_do,                          /* do_it */
  ftp_done,                        /* done */
  ftp_do_more,                     /* do_more */
  ftp_connect,                     /* connect_it */
  ftp_multi_statemach,             /* connecting */
  ftp_doing,                       /* doing */
  ftp_getsock,                     /* proto_getsock */
  ftp_getsock,                     /* doing_getsock */
  ftp_domore_getsock,              /* domore_getsock */
  ZERO_NULL,                       /* perform_getsock */
  ftp_disconnect,                  /* disconnect */
  ZERO_NULL,                       /* readwrite */
  ZERO_NULL,                       /* connection_check */
  PORT_FTP,                        /* defport */
  CURLPROTO_FTP,                   /* protocol */
  PROTOPT_DUAL | PROTOPT_CLOSEACTION | PROTOPT_NEEDSPWD |
  PROTOPT_NOURLQUERY | PROTOPT_PROXY_AS_HTTP |
  PROTOPT_WILDCARD /* flags */
};


#ifdef USE_SSL
/*
 * FTPS protocol handler.
 */

const struct Curl_handler Curl_handler_ftps = {
  "FTPS",                          /* scheme */
  ftp_setup_connection,            /* setup_connection */
  ftp_do,                          /* do_it */
  ftp_done,                        /* done */
  ftp_do_more,                     /* do_more */
  ftp_connect,                     /* connect_it */
  ftp_multi_statemach,             /* connecting */
  ftp_doing,                       /* doing */
  ftp_getsock,                     /* proto_getsock */
  ftp_getsock,                     /* doing_getsock */
  ftp_domore_getsock,              /* domore_getsock */
  ZERO_NULL,                       /* perform_getsock */
  ftp_disconnect,                  /* disconnect */
  ZERO_NULL,                       /* readwrite */
  ZERO_NULL,                       /* connection_check */
  PORT_FTPS,                       /* defport */
  CURLPROTO_FTPS,                  /* protocol */
  PROTOPT_SSL | PROTOPT_DUAL | PROTOPT_CLOSEACTION |
  PROTOPT_NEEDSPWD | PROTOPT_NOURLQUERY | PROTOPT_WILDCARD /* flags */
};
#endif

static void close_secondarysocket(struct connectdata *conn)
{
  if(CURL_SOCKET_BAD != conn->sock[SECONDARYSOCKET]) {
    Curl_closesocket(conn, conn->sock[SECONDARYSOCKET]);
    conn->sock[SECONDARYSOCKET] = CURL_SOCKET_BAD;
  }
  conn->bits.tcpconnect[SECONDARYSOCKET] = FALSE;
}

/*
 * NOTE: back in the old days, we added code in the FTP code that made NOBODY
 * requests on files respond with headers passed to the client/stdout that
 * looked like HTTP ones.
 *
 * This approach is not very elegant, it causes confusion and is error-prone.
 * It is subject for removal at the next (or at least a future) soname bump.
 * Until then you can test the effects of the removal by undefining the
 * following define named CURL_FTP_HTTPSTYLE_HEAD.
 */
#define CURL_FTP_HTTPSTYLE_HEAD 1

static void freedirs(struct ftp_conn *ftpc)
{
  if(ftpc->dirs) {
    int i;
    for(i = 0; i < ftpc->dirdepth; i++) {
      free(ftpc->dirs[i]);
      ftpc->dirs[i] = NULL;
    }
    free(ftpc->dirs);
    ftpc->dirs = NULL;
    ftpc->dirdepth = 0;
  }
  Curl_safefree(ftpc->file);

  /* no longer of any use */
  Curl_safefree(ftpc->newhost);
}

/* Returns non-zero if the given string contains CR (\r) or LF (\n),
   which are not allowed within RFC 959 <string>.
   Note: The input string is in the client's encoding which might
   not be ASCII, so escape sequences \r & \n must be used instead
   of hex values 0x0d & 0x0a.
*/
static bool isBadFtpString(const char *string)
{
  return ((NULL != strchr(string, '\r')) ||
          (NULL != strchr(string, '\n'))) ? TRUE : FALSE;
}

/***********************************************************************
 *
 * AcceptServerConnect()
 *
 * After connection request is received from the server this function is
 * called to accept the connection and close the listening socket
 *
 */
static CURLcode AcceptServerConnect(struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  curl_socket_t sock = conn->sock[SECONDARYSOCKET];
  curl_socket_t s = CURL_SOCKET_BAD;
#ifdef ENABLE_IPV6
  struct Curl_sockaddr_storage add;
#else
  struct sockaddr_in add;
#endif
  curl_socklen_t size = (curl_socklen_t) sizeof(add);

  if(0 == getsockname(sock, (struct sockaddr *) &add, &size)) {
    size = sizeof(add);

    s = accept(sock, (struct sockaddr *) &add, &size);
  }
  Curl_closesocket(conn, sock); /* close the first socket */

  if(CURL_SOCKET_BAD == s) {
    failf(data, "Error accept()ing server connect");
    return CURLE_FTP_PORT_FAILED;
  }
  infof(data, "Connection accepted from server\n");
  /* when this happens within the DO state it is important that we mark us as
     not needing DO_MORE anymore */
  conn->bits.do_more = FALSE;

  conn->sock[SECONDARYSOCKET] = s;
  (void)curlx_nonblock(s, TRUE); /* enable non-blocking */
  conn->sock_accepted[SECONDARYSOCKET] = TRUE;

  if(data->set.fsockopt) {
    int error = 0;

    /* activate callback for setting socket options */
    Curl_set_in_callback(data, true);
    error = data->set.fsockopt(data->set.sockopt_client,
                               s,
                               CURLSOCKTYPE_ACCEPT);
    Curl_set_in_callback(data, false);

    if(error) {
      close_secondarysocket(conn);
      return CURLE_ABORTED_BY_CALLBACK;
    }
  }

  return CURLE_OK;

}

/*
 * ftp_timeleft_accept() returns the amount of milliseconds left allowed for
 * waiting server to connect. If the value is negative, the timeout time has
 * already elapsed.
 *
 * The start time is stored in progress.t_acceptdata - as set with
 * Curl_pgrsTime(..., TIMER_STARTACCEPT);
 *
 */
static timediff_t ftp_timeleft_accept(struct Curl_easy *data)
{
  timediff_t timeout_ms = DEFAULT_ACCEPT_TIMEOUT;
  timediff_t other;
  struct curltime now;

  if(data->set.accepttimeout > 0)
    timeout_ms = data->set.accepttimeout;

  now = Curl_now();

  /* check if the generic timeout possibly is set shorter */
  other =  Curl_timeleft(data, &now, FALSE);
  if(other && (other < timeout_ms))
    /* note that this also works fine for when other happens to be negative
       due to it already having elapsed */
    timeout_ms = other;
  else {
    /* subtract elapsed time */
    timeout_ms -= Curl_timediff(now, data->progress.t_acceptdata);
    if(!timeout_ms)
      /* avoid returning 0 as that means no timeout! */
      return -1;
  }

  return timeout_ms;
}


/***********************************************************************
 *
 * ReceivedServerConnect()
 *
 * After allowing server to connect to us from data port, this function
 * checks both data connection for connection establishment and ctrl
 * connection for a negative response regarding a failure in connecting
 *
 */
static CURLcode ReceivedServerConnect(struct connectdata *conn, bool *received)
{
  struct Curl_easy *data = conn->data;
  curl_socket_t ctrl_sock = conn->sock[FIRSTSOCKET];
  curl_socket_t data_sock = conn->sock[SECONDARYSOCKET];
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;
  int result;
  timediff_t timeout_ms;
  ssize_t nread;
  int ftpcode;

  *received = FALSE;

  timeout_ms = ftp_timeleft_accept(data);
  infof(data, "Checking for server connect\n");
  if(timeout_ms < 0) {
    /* if a timeout was already reached, bail out */
    failf(data, "Accept timeout occurred while waiting server connect");
    return CURLE_FTP_ACCEPT_TIMEOUT;
  }

  /* First check whether there is a cached response from server */
  if(pp->cache_size && pp->cache && pp->cache[0] > '3') {
    /* Data connection could not be established, let's return */
    infof(data, "There is negative response in cache while serv connect\n");
    Curl_GetFTPResponse(&nread, conn, &ftpcode);
    return CURLE_FTP_ACCEPT_FAILED;
  }

  result = Curl_socket_check(ctrl_sock, data_sock, CURL_SOCKET_BAD, 0);

  /* see if the connection request is already here */
  switch(result) {
  case -1: /* error */
    /* let's die here */
    failf(data, "Error while waiting for server connect");
    return CURLE_FTP_ACCEPT_FAILED;
  case 0:  /* Server connect is not received yet */
    break; /* loop */
  default:

    if(result & CURL_CSELECT_IN2) {
      infof(data, "Ready to accept data connection from server\n");
      *received = TRUE;
    }
    else if(result & CURL_CSELECT_IN) {
      infof(data, "Ctrl conn has data while waiting for data conn\n");
      Curl_GetFTPResponse(&nread, conn, &ftpcode);

      if(ftpcode/100 > 3)
        return CURLE_FTP_ACCEPT_FAILED;

      return CURLE_WEIRD_SERVER_REPLY;
    }

    break;
  } /* switch() */

  return CURLE_OK;
}


/***********************************************************************
 *
 * InitiateTransfer()
 *
 * After connection from server is accepted this function is called to
 * setup transfer parameters and initiate the data transfer.
 *
 */
static CURLcode InitiateTransfer(struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  CURLcode result = CURLE_OK;

  if(conn->bits.ftp_use_data_ssl) {
    /* since we only have a plaintext TCP connection here, we must now
     * do the TLS stuff */
    infof(data, "Doing the SSL/TLS handshake on the data stream\n");
    result = Curl_ssl_connect(conn, SECONDARYSOCKET);
    if(result)
      return result;
  }

  if(conn->proto.ftpc.state_saved == FTP_STOR) {
    /* When we know we're uploading a specified file, we can get the file
       size prior to the actual upload. */
    Curl_pgrsSetUploadSize(data, data->state.infilesize);

    /* set the SO_SNDBUF for the secondary socket for those who need it */
    Curl_sndbufset(conn->sock[SECONDARYSOCKET]);

    Curl_setup_transfer(data, -1, -1, FALSE, SECONDARYSOCKET);
  }
  else {
    /* FTP download: */
    Curl_setup_transfer(data, SECONDARYSOCKET,
                        conn->proto.ftpc.retr_size_saved, FALSE, -1);
  }

  conn->proto.ftpc.pp.pending_resp = TRUE; /* expect server response */
  state(conn, FTP_STOP);

  return CURLE_OK;
}

/***********************************************************************
 *
 * AllowServerConnect()
 *
 * When we've issue the PORT command, we have told the server to connect to
 * us. This function checks whether data connection is established if so it is
 * accepted.
 *
 */
static CURLcode AllowServerConnect(struct connectdata *conn, bool *connected)
{
  struct Curl_easy *data = conn->data;
  timediff_t timeout_ms;
  CURLcode result = CURLE_OK;

  *connected = FALSE;
  infof(data, "Preparing for accepting server on data port\n");

  /* Save the time we start accepting server connect */
  Curl_pgrsTime(data, TIMER_STARTACCEPT);

  timeout_ms = ftp_timeleft_accept(data);
  if(timeout_ms < 0) {
    /* if a timeout was already reached, bail out */
    failf(data, "Accept timeout occurred while waiting server connect");
    return CURLE_FTP_ACCEPT_TIMEOUT;
  }

  /* see if the connection request is already here */
  result = ReceivedServerConnect(conn, connected);
  if(result)
    return result;

  if(*connected) {
    result = AcceptServerConnect(conn);
    if(result)
      return result;

    result = InitiateTransfer(conn);
    if(result)
      return result;
  }
  else {
    /* Add timeout to multi handle and break out of the loop */
    if(!result && *connected == FALSE) {
      Curl_expire(data, data->set.accepttimeout > 0 ?
                  data->set.accepttimeout: DEFAULT_ACCEPT_TIMEOUT, 0);
    }
  }

  return result;
}

/* macro to check for a three-digit ftp status code at the start of the
   given string */
#define STATUSCODE(line) (ISDIGIT(line[0]) && ISDIGIT(line[1]) &&       \
                          ISDIGIT(line[2]))

/* macro to check for the last line in an FTP server response */
#define LASTLINE(line) (STATUSCODE(line) && (' ' == line[3]))

static bool ftp_endofresp(struct connectdata *conn, char *line, size_t len,
                          int *code)
{
  (void)conn;

  if((len > 3) && LASTLINE(line)) {
    *code = curlx_sltosi(strtol(line, NULL, 10));
    return TRUE;
  }

  return FALSE;
}

static CURLcode ftp_readresp(curl_socket_t sockfd,
                             struct pingpong *pp,
                             int *ftpcode, /* return the ftp-code if done */
                             size_t *size) /* size of the response */
{
  struct connectdata *conn = pp->conn;
  struct Curl_easy *data = conn->data;
#ifdef HAVE_GSSAPI
  char * const buf = data->state.buffer;
#endif
  int code;
  CURLcode result = Curl_pp_readresp(sockfd, pp, &code, size);

#if defined(HAVE_GSSAPI)
  /* handle the security-oriented responses 6xx ***/
  switch(code) {
  case 631:
    code = Curl_sec_read_msg(conn, buf, PROT_SAFE);
    break;
  case 632:
    code = Curl_sec_read_msg(conn, buf, PROT_PRIVATE);
    break;
  case 633:
    code = Curl_sec_read_msg(conn, buf, PROT_CONFIDENTIAL);
    break;
  default:
    /* normal ftp stuff we pass through! */
    break;
  }
#endif

  /* store the latest code for later retrieval */
  data->info.httpcode = code;

  if(ftpcode)
    *ftpcode = code;

  if(421 == code) {
    /* 421 means "Service not available, closing control connection." and FTP
     * servers use it to signal that idle session timeout has been exceeded.
     * If we ignored the response, it could end up hanging in some cases.
     *
     * This response code can come at any point so having it treated
     * generically is a good idea.
     */
    infof(data, "We got a 421 - timeout!\n");
    state(conn, FTP_STOP);
    return CURLE_OPERATION_TIMEDOUT;
  }

  return result;
}

/* --- parse FTP server responses --- */

/*
 * Curl_GetFTPResponse() is a BLOCKING function to read the full response
 * from a server after a command.
 *
 */

CURLcode Curl_GetFTPResponse(ssize_t *nreadp, /* return number of bytes read */
                             struct connectdata *conn,
                             int *ftpcode) /* return the ftp-code */
{
  /*
   * We cannot read just one byte per read() and then go back to select() as
   * the OpenSSL read() doesn't grok that properly.
   *
   * Alas, read as much as possible, split up into lines, use the ending
   * line in a response or continue reading.  */

  curl_socket_t sockfd = conn->sock[FIRSTSOCKET];
  struct Curl_easy *data = conn->data;
  CURLcode result = CURLE_OK;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;
  size_t nread;
  int cache_skip = 0;
  int value_to_be_ignored = 0;

  if(ftpcode)
    *ftpcode = 0; /* 0 for errors */
  else
    /* make the pointer point to something for the rest of this function */
    ftpcode = &value_to_be_ignored;

  *nreadp = 0;

  while(!*ftpcode && !result) {
    /* check and reset timeout value every lap */
    time_t timeout = Curl_pp_state_timeout(pp, FALSE);
    time_t interval_ms;

    if(timeout <= 0) {
      failf(data, "FTP response timeout");
      return CURLE_OPERATION_TIMEDOUT; /* already too little time */
    }

    interval_ms = 1000;  /* use 1 second timeout intervals */
    if(timeout < interval_ms)
      interval_ms = timeout;

    /*
     * Since this function is blocking, we need to wait here for input on the
     * connection and only then we call the response reading function. We do
     * timeout at least every second to make the timeout check run.
     *
     * A caution here is that the ftp_readresp() function has a cache that may
     * contain pieces of a response from the previous invoke and we need to
     * make sure we don't just wait for input while there is unhandled data in
     * that cache. But also, if the cache is there, we call ftp_readresp() and
     * the cache wasn't good enough to continue we must not just busy-loop
     * around this function.
     *
     */

    if(pp->cache && (cache_skip < 2)) {
      /*
       * There's a cache left since before. We then skipping the wait for
       * socket action, unless this is the same cache like the previous round
       * as then the cache was deemed not enough to act on and we then need to
       * wait for more data anyway.
       */
    }
    else if(!Curl_conn_data_pending(conn, FIRSTSOCKET)) {
      switch(SOCKET_READABLE(sockfd, interval_ms)) {
      case -1: /* select() error, stop reading */
        failf(data, "FTP response aborted due to select/poll error: %d",
              SOCKERRNO);
        return CURLE_RECV_ERROR;

      case 0: /* timeout */
        if(Curl_pgrsUpdate(conn))
          return CURLE_ABORTED_BY_CALLBACK;
        continue; /* just continue in our loop for the timeout duration */

      default: /* for clarity */
        break;
      }
    }
    result = ftp_readresp(sockfd, pp, ftpcode, &nread);
    if(result)
      break;

    if(!nread && pp->cache)
      /* bump cache skip counter as on repeated skips we must wait for more
         data */
      cache_skip++;
    else
      /* when we got data or there is no cache left, we reset the cache skip
         counter */
      cache_skip = 0;

    *nreadp += nread;

  } /* while there's buffer left and loop is requested */

  pp->pending_resp = FALSE;

  return result;
}

#if defined(DEBUGBUILD) && !defined(CURL_DISABLE_VERBOSE_STRINGS)
  /* for debug purposes */
static const char * const ftp_state_names[]={
  "STOP",
  "WAIT220",
  "AUTH",
  "USER",
  "PASS",
  "ACCT",
  "PBSZ",
  "PROT",
  "CCC",
  "PWD",
  "SYST",
  "NAMEFMT",
  "QUOTE",
  "RETR_PREQUOTE",
  "STOR_PREQUOTE",
  "POSTQUOTE",
  "CWD",
  "MKD",
  "MDTM",
  "TYPE",
  "LIST_TYPE",
  "RETR_TYPE",
  "STOR_TYPE",
  "SIZE",
  "RETR_SIZE",
  "STOR_SIZE",
  "REST",
  "RETR_REST",
  "PORT",
  "PRET",
  "PASV",
  "LIST",
  "RETR",
  "STOR",
  "QUIT"
};
#endif

/* This is the ONLY way to change FTP state! */
static void _state(struct connectdata *conn,
                   ftpstate newstate
#ifdef DEBUGBUILD
                   , int lineno
#endif
  )
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;

#if defined(DEBUGBUILD)

#if defined(CURL_DISABLE_VERBOSE_STRINGS)
  (void) lineno;
#else
  if(ftpc->state != newstate)
    infof(conn->data, "FTP %p (line %d) state change from %s to %s\n",
          (void *)ftpc, lineno, ftp_state_names[ftpc->state],
          ftp_state_names[newstate]);
#endif
#endif

  ftpc->state = newstate;
}

static CURLcode ftp_state_user(struct connectdata *conn)
{
  CURLcode result;
  struct FTP *ftp = conn->data->req.protop;
  /* send USER */
  PPSENDF(&conn->proto.ftpc.pp, "USER %s", ftp->user?ftp->user:"");

  state(conn, FTP_USER);
  conn->data->state.ftp_trying_alternative = FALSE;

  return CURLE_OK;
}

static CURLcode ftp_state_pwd(struct connectdata *conn)
{
  CURLcode result;

  /* send PWD to discover our entry point */
  PPSENDF(&conn->proto.ftpc.pp, "%s", "PWD");
  state(conn, FTP_PWD);

  return CURLE_OK;
}

/* For the FTP "protocol connect" and "doing" phases only */
static int ftp_getsock(struct connectdata *conn,
                       curl_socket_t *socks)
{
  return Curl_pp_getsock(&conn->proto.ftpc.pp, socks);
}

/* For the FTP "DO_MORE" phase only */
static int ftp_domore_getsock(struct connectdata *conn, curl_socket_t *socks)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  /* When in DO_MORE state, we could be either waiting for us to connect to a
   * remote site, or we could wait for that site to connect to us. Or just
   * handle ordinary commands.
   */

  if(FTP_STOP == ftpc->state) {
    int bits = GETSOCK_READSOCK(0);

    /* if stopped and still in this state, then we're also waiting for a
       connect on the secondary connection */
    socks[0] = conn->sock[FIRSTSOCKET];

    if(!conn->data->set.ftp_use_port) {
      int s;
      int i;
      /* PORT is used to tell the server to connect to us, and during that we
         don't do happy eyeballs, but we do if we connect to the server */
      for(s = 1, i = 0; i<2; i++) {
        if(conn->tempsock[i] != CURL_SOCKET_BAD) {
          socks[s] = conn->tempsock[i];
          bits |= GETSOCK_WRITESOCK(s++);
        }
      }
    }
    else {
      socks[1] = conn->sock[SECONDARYSOCKET];
      bits |= GETSOCK_WRITESOCK(1) | GETSOCK_READSOCK(1);
    }

    return bits;
  }
  return Curl_pp_getsock(&conn->proto.ftpc.pp, socks);
}

/* This is called after the FTP_QUOTE state is passed.

   ftp_state_cwd() sends the range of CWD commands to the server to change to
   the correct directory. It may also need to send MKD commands to create
   missing ones, if that option is enabled.
*/
static CURLcode ftp_state_cwd(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if(ftpc->cwddone)
    /* already done and fine */
    result = ftp_state_mdtm(conn);
  else {
    ftpc->count2 = 0; /* count2 counts failed CWDs */

    /* count3 is set to allow a MKD to fail once. In the case when first CWD
       fails and then MKD fails (due to another session raced it to create the
       dir) this then allows for a second try to CWD to it */
    ftpc->count3 = (conn->data->set.ftp_create_missing_dirs == 2)?1:0;

    if((conn->data->set.ftp_filemethod == FTPFILE_NOCWD) && !ftpc->cwdcount)
      /* No CWD necessary */
      result = ftp_state_mdtm(conn);
    else if(conn->bits.reuse && ftpc->entrypath) {
      /* This is a re-used connection. Since we change directory to where the
         transfer is taking place, we must first get back to the original dir
         where we ended up after login: */
      ftpc->cwdcount = 0; /* we count this as the first path, then we add one
                             for all upcoming ones in the ftp->dirs[] array */
      PPSENDF(&conn->proto.ftpc.pp, "CWD %s", ftpc->entrypath);
      state(conn, FTP_CWD);
    }
    else {
      if(ftpc->dirdepth) {
        ftpc->cwdcount = 1;
        /* issue the first CWD, the rest is sent when the CWD responses are
           received... */
        PPSENDF(&conn->proto.ftpc.pp, "CWD %s", ftpc->dirs[ftpc->cwdcount -1]);
        state(conn, FTP_CWD);
      }
      else {
        /* No CWD necessary */
        result = ftp_state_mdtm(conn);
      }
    }
  }
  return result;
}

typedef enum {
  EPRT,
  PORT,
  DONE
} ftpport;

static CURLcode ftp_state_use_port(struct connectdata *conn,
                                   ftpport fcmd) /* start with this */

{
  CURLcode result = CURLE_OK;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct Curl_easy *data = conn->data;
  curl_socket_t portsock = CURL_SOCKET_BAD;
  char myhost[256] = "";

  struct Curl_sockaddr_storage ss;
  Curl_addrinfo *res, *ai;
  curl_socklen_t sslen;
  char hbuf[NI_MAXHOST];
  struct sockaddr *sa = (struct sockaddr *)&ss;
  struct sockaddr_in * const sa4 = (void *)sa;
#ifdef ENABLE_IPV6
  struct sockaddr_in6 * const sa6 = (void *)sa;
#endif
  char tmp[1024];
  static const char mode[][5] = { "EPRT", "PORT" };
  int rc;
  int error;
  char *host = NULL;
  char *string_ftpport = data->set.str[STRING_FTPPORT];
  struct Curl_dns_entry *h = NULL;
  unsigned short port_min = 0;
  unsigned short port_max = 0;
  unsigned short port;
  bool possibly_non_local = TRUE;
  char buffer[STRERROR_LEN];
  char *addr = NULL;

  /* Step 1, figure out what is requested,
   * accepted format :
   * (ipv4|ipv6|domain|interface)?(:port(-range)?)?
   */

  if(data->set.str[STRING_FTPPORT] &&
     (strlen(data->set.str[STRING_FTPPORT]) > 1)) {

#ifdef ENABLE_IPV6
    size_t addrlen = INET6_ADDRSTRLEN > strlen(string_ftpport) ?
      INET6_ADDRSTRLEN : strlen(string_ftpport);
#else
    size_t addrlen = INET_ADDRSTRLEN > strlen(string_ftpport) ?
      INET_ADDRSTRLEN : strlen(string_ftpport);
#endif
    char *ip_start = string_ftpport;
    char *ip_end = NULL;
    char *port_start = NULL;
    char *port_sep = NULL;

    addr = calloc(addrlen + 1, 1);
    if(!addr)
      return CURLE_OUT_OF_MEMORY;

#ifdef ENABLE_IPV6
    if(*string_ftpport == '[') {
      /* [ipv6]:port(-range) */
      ip_start = string_ftpport + 1;
      ip_end = strchr(string_ftpport, ']');
      if(ip_end)
        strncpy(addr, ip_start, ip_end - ip_start);
    }
    else
#endif
      if(*string_ftpport == ':') {
        /* :port */
        ip_end = string_ftpport;
      }
      else {
        ip_end = strchr(string_ftpport, ':');
        if(ip_end) {
          /* either ipv6 or (ipv4|domain|interface):port(-range) */
#ifdef ENABLE_IPV6
          if(Curl_inet_pton(AF_INET6, string_ftpport, sa6) == 1) {
            /* ipv6 */
            port_min = port_max = 0;
            strcpy(addr, string_ftpport);
            ip_end = NULL; /* this got no port ! */
          }
          else
#endif
            /* (ipv4|domain|interface):port(-range) */
            strncpy(addr, string_ftpport, ip_end - ip_start);
        }
        else
          /* ipv4|interface */
          strcpy(addr, string_ftpport);
      }

    /* parse the port */
    if(ip_end != NULL) {
      port_start = strchr(ip_end, ':');
      if(port_start) {
        port_min = curlx_ultous(strtoul(port_start + 1, NULL, 10));
        port_sep = strchr(port_start, '-');
        if(port_sep) {
          port_max = curlx_ultous(strtoul(port_sep + 1, NULL, 10));
        }
        else
          port_max = port_min;
      }
    }

    /* correct errors like:
     *  :1234-1230
     *  :-4711,  in this case port_min is (unsigned)-1,
     *           therefore port_min > port_max for all cases
     *           but port_max = (unsigned)-1
     */
    if(port_min > port_max)
      port_min = port_max = 0;


    if(*addr != '\0') {
      /* attempt to get the address of the given interface name */
      switch(Curl_if2ip(conn->ip_addr->ai_family,
                        Curl_ipv6_scope(conn->ip_addr->ai_addr),
                        conn->scope_id, addr, hbuf, sizeof(hbuf))) {
        case IF2IP_NOT_FOUND:
          /* not an interface, use the given string as host name instead */
          host = addr;
          break;
        case IF2IP_AF_NOT_SUPPORTED:
          return CURLE_FTP_PORT_FAILED;
        case IF2IP_FOUND:
          host = hbuf; /* use the hbuf for host name */
      }
    }
    else
      /* there was only a port(-range) given, default the host */
      host = NULL;
  } /* data->set.ftpport */

  if(!host) {
    /* not an interface and not a host name, get default by extracting
       the IP from the control connection */
    sslen = sizeof(ss);
    if(getsockname(conn->sock[FIRSTSOCKET], sa, &sslen)) {
      failf(data, "getsockname() failed: %s",
            Curl_strerror(SOCKERRNO, buffer, sizeof(buffer)));
      free(addr);
      return CURLE_FTP_PORT_FAILED;
    }
    switch(sa->sa_family) {
#ifdef ENABLE_IPV6
    case AF_INET6:
      Curl_inet_ntop(sa->sa_family, &sa6->sin6_addr, hbuf, sizeof(hbuf));
      break;
#endif
    default:
      Curl_inet_ntop(sa->sa_family, &sa4->sin_addr, hbuf, sizeof(hbuf));
      break;
    }
    host = hbuf; /* use this host name */
    possibly_non_local = FALSE; /* we know it is local now */
  }

  /* resolv ip/host to ip */
  rc = Curl_resolv(conn, host, 0, FALSE, &h);
  if(rc == CURLRESOLV_PENDING)
    (void)Curl_resolver_wait_resolv(conn, &h);
  if(h) {
    res = h->addr;
    /* when we return from this function, we can forget about this entry
       to we can unlock it now already */
    Curl_resolv_unlock(data, h);
  } /* (h) */
  else
    res = NULL; /* failure! */

  if(res == NULL) {
    failf(data, "failed to resolve the address provided to PORT: %s", host);
    free(addr);
    return CURLE_FTP_PORT_FAILED;
  }

  free(addr);
  host = NULL;

  /* step 2, create a socket for the requested address */

  portsock = CURL_SOCKET_BAD;
  error = 0;
  for(ai = res; ai; ai = ai->ai_next) {
    result = Curl_socket(conn, ai, NULL, &portsock);
    if(result) {
      error = SOCKERRNO;
      continue;
    }
    break;
  }
  if(!ai) {
    failf(data, "socket failure: %s",
          Curl_strerror(error, buffer, sizeof(buffer)));
    return CURLE_FTP_PORT_FAILED;
  }

  /* step 3, bind to a suitable local address */

  memcpy(sa, ai->ai_addr, ai->ai_addrlen);
  sslen = ai->ai_addrlen;

  for(port = port_min; port <= port_max;) {
    if(sa->sa_family == AF_INET)
      sa4->sin_port = htons(port);
#ifdef ENABLE_IPV6
    else
      sa6->sin6_port = htons(port);
#endif
    /* Try binding the given address. */
    if(bind(portsock, sa, sslen) ) {
      /* It failed. */
      error = SOCKERRNO;
      if(possibly_non_local && (error == EADDRNOTAVAIL)) {
        /* The requested bind address is not local.  Use the address used for
         * the control connection instead and restart the port loop
         */
        infof(data, "bind(port=%hu) on non-local address failed: %s\n", port,
              Curl_strerror(error, buffer, sizeof(buffer)));

        sslen = sizeof(ss);
        if(getsockname(conn->sock[FIRSTSOCKET], sa, &sslen)) {
          failf(data, "getsockname() failed: %s",
                Curl_strerror(SOCKERRNO, buffer, sizeof(buffer)));
          Curl_closesocket(conn, portsock);
          return CURLE_FTP_PORT_FAILED;
        }
        port = port_min;
        possibly_non_local = FALSE; /* don't try this again */
        continue;
      }
      if(error != EADDRINUSE && error != EACCES) {
        failf(data, "bind(port=%hu) failed: %s", port,
              Curl_strerror(error, buffer, sizeof(buffer)));
        Curl_closesocket(conn, portsock);
        return CURLE_FTP_PORT_FAILED;
      }
    }
    else
      break;

    port++;
  }

  /* maybe all ports were in use already*/
  if(port > port_max) {
    failf(data, "bind() failed, we ran out of ports!");
    Curl_closesocket(conn, portsock);
    return CURLE_FTP_PORT_FAILED;
  }

  /* get the name again after the bind() so that we can extract the
     port number it uses now */
  sslen = sizeof(ss);
  if(getsockname(portsock, (struct sockaddr *)sa, &sslen)) {
    failf(data, "getsockname() failed: %s",
          Curl_strerror(SOCKERRNO, buffer, sizeof(buffer)));
    Curl_closesocket(conn, portsock);
    return CURLE_FTP_PORT_FAILED;
  }

  /* step 4, listen on the socket */

  if(listen(portsock, 1)) {
    failf(data, "socket failure: %s",
          Curl_strerror(SOCKERRNO, buffer, sizeof(buffer)));
    Curl_closesocket(conn, portsock);
    return CURLE_FTP_PORT_FAILED;
  }

  /* step 5, send the proper FTP command */

  /* get a plain printable version of the numerical address to work with
     below */
  Curl_printable_address(ai, myhost, sizeof(myhost));

#ifdef ENABLE_IPV6
  if(!conn->bits.ftp_use_eprt && conn->bits.ipv6)
    /* EPRT is disabled but we are connected to a IPv6 host, so we ignore the
       request and enable EPRT again! */
    conn->bits.ftp_use_eprt = TRUE;
#endif

  for(; fcmd != DONE; fcmd++) {

    if(!conn->bits.ftp_use_eprt && (EPRT == fcmd))
      /* if disabled, goto next */
      continue;

    if((PORT == fcmd) && sa->sa_family != AF_INET)
      /* PORT is IPv4 only */
      continue;

    switch(sa->sa_family) {
    case AF_INET:
      port = ntohs(sa4->sin_port);
      break;
#ifdef ENABLE_IPV6
    case AF_INET6:
      port = ntohs(sa6->sin6_port);
      break;
#endif
    default:
      continue; /* might as well skip this */
    }

    if(EPRT == fcmd) {
      /*
       * Two fine examples from RFC2428;
       *
       * EPRT |1|132.235.1.2|6275|
       *
       * EPRT |2|1080::8:800:200C:417A|5282|
       */

      result = Curl_pp_sendf(&ftpc->pp, "%s |%d|%s|%hu|", mode[fcmd],
                             sa->sa_family == AF_INET?1:2,
                             myhost, port);
      if(result) {
        failf(data, "Failure sending EPRT command: %s",
              curl_easy_strerror(result));
        Curl_closesocket(conn, portsock);
        /* don't retry using PORT */
        ftpc->count1 = PORT;
        /* bail out */
        state(conn, FTP_STOP);
        return result;
      }
      break;
    }
    if(PORT == fcmd) {
      char *source = myhost;
      char *dest = tmp;

      /* translate x.x.x.x to x,x,x,x */
      while(source && *source) {
        if(*source == '.')
          *dest = ',';
        else
          *dest = *source;
        dest++;
        source++;
      }
      *dest = 0;
      msnprintf(dest, 20, ",%d,%d", (int)(port>>8), (int)(port&0xff));

      result = Curl_pp_sendf(&ftpc->pp, "%s %s", mode[fcmd], tmp);
      if(result) {
        failf(data, "Failure sending PORT command: %s",
              curl_easy_strerror(result));
        Curl_closesocket(conn, portsock);
        /* bail out */
        state(conn, FTP_STOP);
        return result;
      }
      break;
    }
  }

  /* store which command was sent */
  ftpc->count1 = fcmd;

  close_secondarysocket(conn);

  /* we set the secondary socket variable to this for now, it is only so that
     the cleanup function will close it in case we fail before the true
     secondary stuff is made */
  conn->sock[SECONDARYSOCKET] = portsock;

  /* this tcpconnect assignment below is a hackish work-around to make the
     multi interface with active FTP work - as it will not wait for a
     (passive) connect in Curl_is_connected().

     The *proper* fix is to make sure that the active connection from the
     server is done in a non-blocking way. Currently, it is still BLOCKING.
  */
  conn->bits.tcpconnect[SECONDARYSOCKET] = TRUE;

  state(conn, FTP_PORT);
  return result;
}

static CURLcode ftp_state_use_pasv(struct connectdata *conn)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  CURLcode result = CURLE_OK;
  /*
    Here's the excecutive summary on what to do:

    PASV is RFC959, expect:
    227 Entering Passive Mode (a1,a2,a3,a4,p1,p2)

    LPSV is RFC1639, expect:
    228 Entering Long Passive Mode (4,4,a1,a2,a3,a4,2,p1,p2)

    EPSV is RFC2428, expect:
    229 Entering Extended Passive Mode (|||port|)

  */

  static const char mode[][5] = { "EPSV", "PASV" };
  int modeoff;

#ifdef PF_INET6
  if(!conn->bits.ftp_use_epsv && conn->bits.ipv6)
    /* EPSV is disabled but we are connected to a IPv6 host, so we ignore the
       request and enable EPSV again! */
    conn->bits.ftp_use_epsv = TRUE;
#endif

  modeoff = conn->bits.ftp_use_epsv?0:1;

  PPSENDF(&ftpc->pp, "%s", mode[modeoff]);

  ftpc->count1 = modeoff;
  state(conn, FTP_PASV);
  infof(conn->data, "Connect data stream passively\n");

  return result;
}

/*
 * ftp_state_prepare_transfer() starts PORT, PASV or PRET etc.
 *
 * REST is the last command in the chain of commands when a "head"-like
 * request is made. Thus, if an actual transfer is to be made this is where we
 * take off for real.
 */
static CURLcode ftp_state_prepare_transfer(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct FTP *ftp = conn->data->req.protop;
  struct Curl_easy *data = conn->data;

  if(ftp->transfer != FTPTRANSFER_BODY) {
    /* doesn't transfer any data */

    /* still possibly do PRE QUOTE jobs */
    state(conn, FTP_RETR_PREQUOTE);
    result = ftp_state_quote(conn, TRUE, FTP_RETR_PREQUOTE);
  }
  else if(data->set.ftp_use_port) {
    /* We have chosen to use the PORT (or similar) command */
    result = ftp_state_use_port(conn, EPRT);
  }
  else {
    /* We have chosen (this is default) to use the PASV (or similar) command */
    if(data->set.ftp_use_pret) {
      /* The user has requested that we send a PRET command
         to prepare the server for the upcoming PASV */
      if(!conn->proto.ftpc.file) {
        PPSENDF(&conn->proto.ftpc.pp, "PRET %s",
                data->set.str[STRING_CUSTOMREQUEST]?
                data->set.str[STRING_CUSTOMREQUEST]:
                (data->set.ftp_list_only?"NLST":"LIST"));
      }
      else if(data->set.upload) {
        PPSENDF(&conn->proto.ftpc.pp, "PRET STOR %s", conn->proto.ftpc.file);
      }
      else {
        PPSENDF(&conn->proto.ftpc.pp, "PRET RETR %s", conn->proto.ftpc.file);
      }
      state(conn, FTP_PRET);
    }
    else {
      result = ftp_state_use_pasv(conn);
    }
  }
  return result;
}

static CURLcode ftp_state_rest(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct FTP *ftp = conn->data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if((ftp->transfer != FTPTRANSFER_BODY) && ftpc->file) {
    /* if a "head"-like request is being made (on a file) */

    /* Determine if server can respond to REST command and therefore
       whether it supports range */
    PPSENDF(&conn->proto.ftpc.pp, "REST %d", 0);

    state(conn, FTP_REST);
  }
  else
    result = ftp_state_prepare_transfer(conn);

  return result;
}

static CURLcode ftp_state_size(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct FTP *ftp = conn->data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if((ftp->transfer == FTPTRANSFER_INFO) && ftpc->file) {
    /* if a "head"-like request is being made (on a file) */

    /* we know ftpc->file is a valid pointer to a file name */
    PPSENDF(&ftpc->pp, "SIZE %s", ftpc->file);

    state(conn, FTP_SIZE);
  }
  else
    result = ftp_state_rest(conn);

  return result;
}

static CURLcode ftp_state_list(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;

  /* If this output is to be machine-parsed, the NLST command might be better
     to use, since the LIST command output is not specified or standard in any
     way. It has turned out that the NLST list output is not the same on all
     servers either... */

  /*
     if FTPFILE_NOCWD was specified, we are currently in
     the user's home directory, so we should add the path
     as argument for the LIST / NLST / or custom command.
     Whether the server will support this, is uncertain.

     The other ftp_filemethods will CWD into dir/dir/ first and
     then just do LIST (in that case: nothing to do here)
  */
  char *cmd, *lstArg, *slashPos;
  const char *inpath = ftp->path;

  lstArg = NULL;
  if((data->set.ftp_filemethod == FTPFILE_NOCWD) &&
     inpath && inpath[0] && strchr(inpath, '/')) {
    size_t n = strlen(inpath);

    /* Check if path does not end with /, as then we cut off the file part */
    if(inpath[n - 1] != '/') {
      /* chop off the file part if format is dir/dir/file */
      slashPos = strrchr(inpath, '/');
      n = slashPos - inpath;
    }
    result = Curl_urldecode(data, inpath, n, &lstArg, NULL, TRUE);
    if(result)
      return result;
  }

  cmd = aprintf("%s%s%s",
                data->set.str[STRING_CUSTOMREQUEST]?
                data->set.str[STRING_CUSTOMREQUEST]:
                (data->set.ftp_list_only?"NLST":"LIST"),
                lstArg? " ": "",
                lstArg? lstArg: "");

  if(!cmd) {
    free(lstArg);
    return CURLE_OUT_OF_MEMORY;
  }

  result = Curl_pp_sendf(&conn->proto.ftpc.pp, "%s", cmd);

  free(lstArg);
  free(cmd);

  if(result)
    return result;

  state(conn, FTP_LIST);

  return result;
}

static CURLcode ftp_state_retr_prequote(struct connectdata *conn)
{
  /* We've sent the TYPE, now we must send the list of prequote strings */
  return ftp_state_quote(conn, TRUE, FTP_RETR_PREQUOTE);
}

static CURLcode ftp_state_stor_prequote(struct connectdata *conn)
{
  /* We've sent the TYPE, now we must send the list of prequote strings */
  return ftp_state_quote(conn, TRUE, FTP_STOR_PREQUOTE);
}

static CURLcode ftp_state_type(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct FTP *ftp = conn->data->req.protop;
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  /* If we have selected NOBODY and HEADER, it means that we only want file
     information. Which in FTP can't be much more than the file size and
     date. */
  if(data->set.opt_no_body && ftpc->file &&
     ftp_need_type(conn, data->set.prefer_ascii)) {
    /* The SIZE command is _not_ RFC 959 specified, and therefore many servers
       may not support it! It is however the only way we have to get a file's
       size! */

    ftp->transfer = FTPTRANSFER_INFO;
    /* this means no actual transfer will be made */

    /* Some servers return different sizes for different modes, and thus we
       must set the proper type before we check the size */
    result = ftp_nb_type(conn, data->set.prefer_ascii, FTP_TYPE);
    if(result)
      return result;
  }
  else
    result = ftp_state_size(conn);

  return result;
}

/* This is called after the CWD commands have been done in the beginning of
   the DO phase */
static CURLcode ftp_state_mdtm(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  /* Requested time of file or time-depended transfer? */
  if((data->set.get_filetime || data->set.timecondition) && ftpc->file) {

    /* we have requested to get the modified-time of the file, this is a white
       spot as the MDTM is not mentioned in RFC959 */
    PPSENDF(&ftpc->pp, "MDTM %s", ftpc->file);

    state(conn, FTP_MDTM);
  }
  else
    result = ftp_state_type(conn);

  return result;
}


/* This is called after the TYPE and possible quote commands have been sent */
static CURLcode ftp_state_ul_setup(struct connectdata *conn,
                                   bool sizechecked)
{
  CURLcode result = CURLE_OK;
  struct FTP *ftp = conn->data->req.protop;
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if((data->state.resume_from && !sizechecked) ||
     ((data->state.resume_from > 0) && sizechecked)) {
    /* we're about to continue the uploading of a file */
    /* 1. get already existing file's size. We use the SIZE command for this
       which may not exist in the server!  The SIZE command is not in
       RFC959. */

    /* 2. This used to set REST. But since we can do append, we
       don't another ftp command. We just skip the source file
       offset and then we APPEND the rest on the file instead */

    /* 3. pass file-size number of bytes in the source file */
    /* 4. lower the infilesize counter */
    /* => transfer as usual */
    int seekerr = CURL_SEEKFUNC_OK;

    if(data->state.resume_from < 0) {
      /* Got no given size to start from, figure it out */
      PPSENDF(&ftpc->pp, "SIZE %s", ftpc->file);
      state(conn, FTP_STOR_SIZE);
      return result;
    }

    /* enable append */
    data->set.ftp_append = TRUE;

    /* Let's read off the proper amount of bytes from the input. */
    if(conn->seek_func) {
      Curl_set_in_callback(data, true);
      seekerr = conn->seek_func(conn->seek_client, data->state.resume_from,
                                SEEK_SET);
      Curl_set_in_callback(data, false);
    }

    if(seekerr != CURL_SEEKFUNC_OK) {
      curl_off_t passed = 0;
      if(seekerr != CURL_SEEKFUNC_CANTSEEK) {
        failf(data, "Could not seek stream");
        return CURLE_FTP_COULDNT_USE_REST;
      }
      /* seekerr == CURL_SEEKFUNC_CANTSEEK (can't seek to offset) */
      do {
        size_t readthisamountnow =
          (data->state.resume_from - passed > data->set.buffer_size) ?
          (size_t)data->set.buffer_size :
          curlx_sotouz(data->state.resume_from - passed);

        size_t actuallyread =
          data->state.fread_func(data->state.buffer, 1, readthisamountnow,
                                 data->state.in);

        passed += actuallyread;
        if((actuallyread == 0) || (actuallyread > readthisamountnow)) {
          /* this checks for greater-than only to make sure that the
             CURL_READFUNC_ABORT return code still aborts */
          failf(data, "Failed to read data");
          return CURLE_FTP_COULDNT_USE_REST;
        }
      } while(passed < data->state.resume_from);
    }
    /* now, decrease the size of the read */
    if(data->state.infilesize>0) {
      data->state.infilesize -= data->state.resume_from;

      if(data->state.infilesize <= 0) {
        infof(data, "File already completely uploaded\n");

        /* no data to transfer */
        Curl_setup_transfer(data, -1, -1, FALSE, -1);

        /* Set ->transfer so that we won't get any error in
         * ftp_done() because we didn't transfer anything! */
        ftp->transfer = FTPTRANSFER_NONE;

        state(conn, FTP_STOP);
        return CURLE_OK;
      }
    }
    /* we've passed, proceed as normal */
  } /* resume_from */

  PPSENDF(&ftpc->pp, data->set.ftp_append?"APPE %s":"STOR %s",
          ftpc->file);

  state(conn, FTP_STOR);

  return result;
}

static CURLcode ftp_state_quote(struct connectdata *conn,
                                bool init,
                                ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  bool quote = FALSE;
  struct curl_slist *item;

  switch(instate) {
  case FTP_QUOTE:
  default:
    item = data->set.quote;
    break;
  case FTP_RETR_PREQUOTE:
  case FTP_STOR_PREQUOTE:
    item = data->set.prequote;
    break;
  case FTP_POSTQUOTE:
    item = data->set.postquote;
    break;
  }

  /*
   * This state uses:
   * 'count1' to iterate over the commands to send
   * 'count2' to store whether to allow commands to fail
   */

  if(init)
    ftpc->count1 = 0;
  else
    ftpc->count1++;

  if(item) {
    int i = 0;

    /* Skip count1 items in the linked list */
    while((i< ftpc->count1) && item) {
      item = item->next;
      i++;
    }
    if(item) {
      char *cmd = item->data;
      if(cmd[0] == '*') {
        cmd++;
        ftpc->count2 = 1; /* the sent command is allowed to fail */
      }
      else
        ftpc->count2 = 0; /* failure means cancel operation */

      PPSENDF(&ftpc->pp, "%s", cmd);
      state(conn, instate);
      quote = TRUE;
    }
  }

  if(!quote) {
    /* No more quote to send, continue to ... */
    switch(instate) {
    case FTP_QUOTE:
    default:
      result = ftp_state_cwd(conn);
      break;
    case FTP_RETR_PREQUOTE:
      if(ftp->transfer != FTPTRANSFER_BODY)
        state(conn, FTP_STOP);
      else {
        if(ftpc->known_filesize != -1) {
          Curl_pgrsSetDownloadSize(data, ftpc->known_filesize);
          result = ftp_state_retr(conn, ftpc->known_filesize);
        }
        else {
          if(data->set.ignorecl) {
            /* This code is to support download of growing files.  It prevents
               the state machine from requesting the file size from the
               server.  With an unknown file size the download continues until
               the server terminates it, otherwise the client stops if the
               received byte count exceeds the reported file size.  Set option
               CURLOPT_IGNORE_CONTENT_LENGTH to 1 to enable this behavior.*/
            PPSENDF(&ftpc->pp, "RETR %s", ftpc->file);
            state(conn, FTP_RETR);
          }
          else {
            PPSENDF(&ftpc->pp, "SIZE %s", ftpc->file);
            state(conn, FTP_RETR_SIZE);
          }
        }
      }
      break;
    case FTP_STOR_PREQUOTE:
      result = ftp_state_ul_setup(conn, FALSE);
      break;
    case FTP_POSTQUOTE:
      break;
    }
  }

  return result;
}

/* called from ftp_state_pasv_resp to switch to PASV in case of EPSV
   problems */
static CURLcode ftp_epsv_disable(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;

  if(conn->bits.ipv6 && !(conn->bits.tunnel_proxy || conn->bits.socksproxy)) {
    /* We can't disable EPSV when doing IPv6, so this is instead a fail */
    failf(conn->data, "Failed EPSV attempt, exiting\n");
    return CURLE_WEIRD_SERVER_REPLY;
  }

  infof(conn->data, "Failed EPSV attempt. Disabling EPSV\n");
  /* disable it for next transfer */
  conn->bits.ftp_use_epsv = FALSE;
  conn->data->state.errorbuf = FALSE; /* allow error message to get
                                         rewritten */
  PPSENDF(&conn->proto.ftpc.pp, "%s", "PASV");
  conn->proto.ftpc.count1++;
  /* remain in/go to the FTP_PASV state */
  state(conn, FTP_PASV);
  return result;
}


static char *control_address(struct connectdata *conn)
{
  /* Returns the control connection IP address.
     If a proxy tunnel is used, returns the original host name instead, because
     the effective control connection address is the proxy address,
     not the ftp host. */
  if(conn->bits.tunnel_proxy || conn->bits.socksproxy)
    return conn->host.name;

  return conn->ip_addr_str;
}

static CURLcode ftp_state_pasv_resp(struct connectdata *conn,
                                    int ftpcode)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  CURLcode result;
  struct Curl_easy *data = conn->data;
  struct Curl_dns_entry *addr = NULL;
  int rc;
  unsigned short connectport; /* the local port connect() should use! */
  char *str = &data->state.buffer[4];  /* start on the first letter */

  /* if we come here again, make sure the former name is cleared */
  Curl_safefree(ftpc->newhost);

  if((ftpc->count1 == 0) &&
     (ftpcode == 229)) {
    /* positive EPSV response */
    char *ptr = strchr(str, '(');
    if(ptr) {
      unsigned int num;
      char separator[4];
      ptr++;
      if(5 == sscanf(ptr, "%c%c%c%u%c",
                     &separator[0],
                     &separator[1],
                     &separator[2],
                     &num,
                     &separator[3])) {
        const char sep1 = separator[0];
        int i;

        /* The four separators should be identical, or else this is an oddly
           formatted reply and we bail out immediately. */
        for(i = 1; i<4; i++) {
          if(separator[i] != sep1) {
            ptr = NULL; /* set to NULL to signal error */
            break;
          }
        }
        if(num > 0xffff) {
          failf(data, "Illegal port number in EPSV reply");
          return CURLE_FTP_WEIRD_PASV_REPLY;
        }
        if(ptr) {
          ftpc->newport = (unsigned short)(num & 0xffff);
          ftpc->newhost = strdup(control_address(conn));
          if(!ftpc->newhost)
            return CURLE_OUT_OF_MEMORY;
        }
      }
      else
        ptr = NULL;
    }
    if(!ptr) {
      failf(data, "Weirdly formatted EPSV reply");
      return CURLE_FTP_WEIRD_PASV_REPLY;
    }
  }
  else if((ftpc->count1 == 1) &&
          (ftpcode == 227)) {
    /* positive PASV response */
    unsigned int ip[4];
    unsigned int port[2];

    /*
     * Scan for a sequence of six comma-separated numbers and use them as
     * IP+port indicators.
     *
     * Found reply-strings include:
     * "227 Entering Passive Mode (127,0,0,1,4,51)"
     * "227 Data transfer will passively listen to 127,0,0,1,4,51"
     * "227 Entering passive mode. 127,0,0,1,4,51"
     */
    while(*str) {
      if(6 == sscanf(str, "%u,%u,%u,%u,%u,%u",
                     &ip[0], &ip[1], &ip[2], &ip[3],
                     &port[0], &port[1]))
        break;
      str++;
    }

    if(!*str || (ip[0] > 255) || (ip[1] > 255)  || (ip[2] > 255)  ||
       (ip[3] > 255) || (port[0] > 255)  || (port[1] > 255) ) {
      failf(data, "Couldn't interpret the 227-response");
      return CURLE_FTP_WEIRD_227_FORMAT;
    }

    /* we got OK from server */
    if(data->set.ftp_skip_ip) {
      /* told to ignore the remotely given IP but instead use the host we used
         for the control connection */
      infof(data, "Skip %u.%u.%u.%u for data connection, re-use %s instead\n",
            ip[0], ip[1], ip[2], ip[3],
            conn->host.name);
      ftpc->newhost = strdup(control_address(conn));
    }
    else
      ftpc->newhost = aprintf("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    if(!ftpc->newhost)
      return CURLE_OUT_OF_MEMORY;

    ftpc->newport = (unsigned short)(((port[0]<<8) + port[1]) & 0xffff);
  }
  else if(ftpc->count1 == 0) {
    /* EPSV failed, move on to PASV */
    return ftp_epsv_disable(conn);
  }
  else {
    failf(data, "Bad PASV/EPSV response: %03d", ftpcode);
    return CURLE_FTP_WEIRD_PASV_REPLY;
  }

  if(conn->bits.proxy) {
    /*
     * This connection uses a proxy and we need to connect to the proxy again
     * here. We don't want to rely on a former host lookup that might've
     * expired now, instead we remake the lookup here and now!
     */
    const char * const host_name = conn->bits.socksproxy ?
      conn->socks_proxy.host.name : conn->http_proxy.host.name;
    rc = Curl_resolv(conn, host_name, (int)conn->port, FALSE, &addr);
    if(rc == CURLRESOLV_PENDING)
      /* BLOCKING, ignores the return code but 'addr' will be NULL in
         case of failure */
      (void)Curl_resolver_wait_resolv(conn, &addr);

    connectport =
      (unsigned short)conn->port; /* we connect to the proxy's port */

    if(!addr) {
      failf(data, "Can't resolve proxy host %s:%hu", host_name, connectport);
      return CURLE_COULDNT_RESOLVE_PROXY;
    }
  }
  else {
    /* normal, direct, ftp connection */
    rc = Curl_resolv(conn, ftpc->newhost, ftpc->newport, FALSE, &addr);
    if(rc == CURLRESOLV_PENDING)
      /* BLOCKING */
      (void)Curl_resolver_wait_resolv(conn, &addr);

    connectport = ftpc->newport; /* we connect to the remote port */

    if(!addr) {
      failf(data, "Can't resolve new host %s:%hu", ftpc->newhost, connectport);
      return CURLE_FTP_CANT_GET_HOST;
    }
  }

  conn->bits.tcpconnect[SECONDARYSOCKET] = FALSE;
  result = Curl_connecthost(conn, addr);

  if(result) {
    Curl_resolv_unlock(data, addr); /* we're done using this address */
    if(ftpc->count1 == 0 && ftpcode == 229)
      return ftp_epsv_disable(conn);

    return result;
  }


  /*
   * When this is used from the multi interface, this might've returned with
   * the 'connected' set to FALSE and thus we are now awaiting a non-blocking
   * connect to connect.
   */

  if(data->set.verbose)
    /* this just dumps information about this second connection */
    ftp_pasv_verbose(conn, addr->addr, ftpc->newhost, connectport);

  Curl_resolv_unlock(data, addr); /* we're done using this address */

  Curl_safefree(conn->secondaryhostname);
  conn->secondary_port = ftpc->newport;
  conn->secondaryhostname = strdup(ftpc->newhost);
  if(!conn->secondaryhostname)
    return CURLE_OUT_OF_MEMORY;

  conn->bits.do_more = TRUE;
  state(conn, FTP_STOP); /* this phase is completed */

  return result;
}

static CURLcode ftp_state_port_resp(struct connectdata *conn,
                                    int ftpcode)
{
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  ftpport fcmd = (ftpport)ftpc->count1;
  CURLcode result = CURLE_OK;

  /* The FTP spec tells a positive response should have code 200.
     Be more permissive here to tolerate deviant servers. */
  if(ftpcode / 100 != 2) {
    /* the command failed */

    if(EPRT == fcmd) {
      infof(data, "disabling EPRT usage\n");
      conn->bits.ftp_use_eprt = FALSE;
    }
    fcmd++;

    if(fcmd == DONE) {
      failf(data, "Failed to do PORT");
      result = CURLE_FTP_PORT_FAILED;
    }
    else
      /* try next */
      result = ftp_state_use_port(conn, fcmd);
  }
  else {
    infof(data, "Connect data stream actively\n");
    state(conn, FTP_STOP); /* end of DO phase */
    result = ftp_dophase_done(conn, FALSE);
  }

  return result;
}

static CURLcode ftp_state_mdtm_resp(struct connectdata *conn,
                                    int ftpcode)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  switch(ftpcode) {
  case 213:
    {
      /* we got a time. Format should be: "YYYYMMDDHHMMSS[.sss]" where the
         last .sss part is optional and means fractions of a second */
      int year, month, day, hour, minute, second;
      if(6 == sscanf(&data->state.buffer[4], "%04d%02d%02d%02d%02d%02d",
                     &year, &month, &day, &hour, &minute, &second)) {
        /* we have a time, reformat it */
        char timebuf[24];
        time_t secs = time(NULL);

        msnprintf(timebuf, sizeof(timebuf),
                  "%04d%02d%02d %02d:%02d:%02d GMT",
                  year, month, day, hour, minute, second);
        /* now, convert this into a time() value: */
        data->info.filetime = curl_getdate(timebuf, &secs);
      }

#ifdef CURL_FTP_HTTPSTYLE_HEAD
      /* If we asked for a time of the file and we actually got one as well,
         we "emulate" a HTTP-style header in our output. */

      if(data->set.opt_no_body &&
         ftpc->file &&
         data->set.get_filetime &&
         (data->info.filetime >= 0) ) {
        char headerbuf[128];
        time_t filetime = data->info.filetime;
        struct tm buffer;
        const struct tm *tm = &buffer;

        result = Curl_gmtime(filetime, &buffer);
        if(result)
          return result;

        /* format: "Tue, 15 Nov 1994 12:45:26" */
        msnprintf(headerbuf, sizeof(headerbuf),
                  "Last-Modified: %s, %02d %s %4d %02d:%02d:%02d GMT\r\n",
                  Curl_wkday[tm->tm_wday?tm->tm_wday-1:6],
                  tm->tm_mday,
                  Curl_month[tm->tm_mon],
                  tm->tm_year + 1900,
                  tm->tm_hour,
                  tm->tm_min,
                  tm->tm_sec);
        result = Curl_client_write(conn, CLIENTWRITE_BOTH, headerbuf, 0);
        if(result)
          return result;
      } /* end of a ridiculous amount of conditionals */
#endif
    }
    break;
  default:
    infof(data, "unsupported MDTM reply format\n");
    break;
  case 550: /* "No such file or directory" */
    failf(data, "Given file does not exist");
    result = CURLE_FTP_COULDNT_RETR_FILE;
    break;
  }

  if(data->set.timecondition) {
    if((data->info.filetime > 0) && (data->set.timevalue > 0)) {
      switch(data->set.timecondition) {
      case CURL_TIMECOND_IFMODSINCE:
      default:
        if(data->info.filetime <= data->set.timevalue) {
          infof(data, "The requested document is not new enough\n");
          ftp->transfer = FTPTRANSFER_NONE; /* mark to not transfer data */
          data->info.timecond = TRUE;
          state(conn, FTP_STOP);
          return CURLE_OK;
        }
        break;
      case CURL_TIMECOND_IFUNMODSINCE:
        if(data->info.filetime > data->set.timevalue) {
          infof(data, "The requested document is not old enough\n");
          ftp->transfer = FTPTRANSFER_NONE; /* mark to not transfer data */
          data->info.timecond = TRUE;
          state(conn, FTP_STOP);
          return CURLE_OK;
        }
        break;
      } /* switch */
    }
    else {
      infof(data, "Skipping time comparison\n");
    }
  }

  if(!result)
    result = ftp_state_type(conn);

  return result;
}

static CURLcode ftp_state_type_resp(struct connectdata *conn,
                                    int ftpcode,
                                    ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;

  if(ftpcode/100 != 2) {
    /* "sasserftpd" and "(u)r(x)bot ftpd" both responds with 226 after a
       successful 'TYPE I'. While that is not as RFC959 says, it is still a
       positive response code and we allow that. */
    failf(data, "Couldn't set desired mode");
    return CURLE_FTP_COULDNT_SET_TYPE;
  }
  if(ftpcode != 200)
    infof(data, "Got a %03d response code instead of the assumed 200\n",
          ftpcode);

  if(instate == FTP_TYPE)
    result = ftp_state_size(conn);
  else if(instate == FTP_LIST_TYPE)
    result = ftp_state_list(conn);
  else if(instate == FTP_RETR_TYPE)
    result = ftp_state_retr_prequote(conn);
  else if(instate == FTP_STOR_TYPE)
    result = ftp_state_stor_prequote(conn);

  return result;
}

static CURLcode ftp_state_retr(struct connectdata *conn,
                                         curl_off_t filesize)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if(data->set.max_filesize && (filesize > data->set.max_filesize)) {
    failf(data, "Maximum file size exceeded");
    return CURLE_FILESIZE_EXCEEDED;
  }
  ftp->downloadsize = filesize;

  if(data->state.resume_from) {
    /* We always (attempt to) get the size of downloads, so it is done before
       this even when not doing resumes. */
    if(filesize == -1) {
      infof(data, "ftp server doesn't support SIZE\n");
      /* We couldn't get the size and therefore we can't know if there really
         is a part of the file left to get, although the server will just
         close the connection when we start the connection so it won't cause
         us any harm, just not make us exit as nicely. */
    }
    else {
      /* We got a file size report, so we check that there actually is a
         part of the file left to get, or else we go home.  */
      if(data->state.resume_from< 0) {
        /* We're supposed to download the last abs(from) bytes */
        if(filesize < -data->state.resume_from) {
          failf(data, "Offset (%" CURL_FORMAT_CURL_OFF_T
                ") was beyond file size (%" CURL_FORMAT_CURL_OFF_T ")",
                data->state.resume_from, filesize);
          return CURLE_BAD_DOWNLOAD_RESUME;
        }
        /* convert to size to download */
        ftp->downloadsize = -data->state.resume_from;
        /* download from where? */
        data->state.resume_from = filesize - ftp->downloadsize;
      }
      else {
        if(filesize < data->state.resume_from) {
          failf(data, "Offset (%" CURL_FORMAT_CURL_OFF_T
                ") was beyond file size (%" CURL_FORMAT_CURL_OFF_T ")",
                data->state.resume_from, filesize);
          return CURLE_BAD_DOWNLOAD_RESUME;
        }
        /* Now store the number of bytes we are expected to download */
        ftp->downloadsize = filesize-data->state.resume_from;
      }
    }

    if(ftp->downloadsize == 0) {
      /* no data to transfer */
      Curl_setup_transfer(data, -1, -1, FALSE, -1);
      infof(data, "File already completely downloaded\n");

      /* Set ->transfer so that we won't get any error in ftp_done()
       * because we didn't transfer the any file */
      ftp->transfer = FTPTRANSFER_NONE;
      state(conn, FTP_STOP);
      return CURLE_OK;
    }

    /* Set resume file transfer offset */
    infof(data, "Instructs server to resume from offset %"
          CURL_FORMAT_CURL_OFF_T "\n", data->state.resume_from);

    PPSENDF(&ftpc->pp, "REST %" CURL_FORMAT_CURL_OFF_T,
            data->state.resume_from);

    state(conn, FTP_RETR_REST);
  }
  else {
    /* no resume */
    PPSENDF(&ftpc->pp, "RETR %s", ftpc->file);
    state(conn, FTP_RETR);
  }

  return result;
}

static CURLcode ftp_state_size_resp(struct connectdata *conn,
                                    int ftpcode,
                                    ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  curl_off_t filesize = -1;
  char *buf = data->state.buffer;

  /* get the size from the ascii string: */
  if(ftpcode == 213)
    /* ignores parsing errors, which will make the size remain unknown */
    (void)curlx_strtoofft(buf + 4, NULL, 0, &filesize);

  if(instate == FTP_SIZE) {
#ifdef CURL_FTP_HTTPSTYLE_HEAD
    if(-1 != filesize) {
      char clbuf[128];
      msnprintf(clbuf, sizeof(clbuf),
                "Content-Length: %" CURL_FORMAT_CURL_OFF_T "\r\n", filesize);
      result = Curl_client_write(conn, CLIENTWRITE_BOTH, clbuf, 0);
      if(result)
        return result;
    }
#endif
    Curl_pgrsSetDownloadSize(data, filesize);
    result = ftp_state_rest(conn);
  }
  else if(instate == FTP_RETR_SIZE) {
    Curl_pgrsSetDownloadSize(data, filesize);
    result = ftp_state_retr(conn, filesize);
  }
  else if(instate == FTP_STOR_SIZE) {
    data->state.resume_from = filesize;
    result = ftp_state_ul_setup(conn, TRUE);
  }

  return result;
}

static CURLcode ftp_state_rest_resp(struct connectdata *conn,
                                    int ftpcode,
                                    ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  switch(instate) {
  case FTP_REST:
  default:
#ifdef CURL_FTP_HTTPSTYLE_HEAD
    if(ftpcode == 350) {
      char buffer[24]= { "Accept-ranges: bytes\r\n" };
      result = Curl_client_write(conn, CLIENTWRITE_BOTH, buffer, 0);
      if(result)
        return result;
    }
#endif
    result = ftp_state_prepare_transfer(conn);
    break;

  case FTP_RETR_REST:
    if(ftpcode != 350) {
      failf(conn->data, "Couldn't use REST");
      result = CURLE_FTP_COULDNT_USE_REST;
    }
    else {
      PPSENDF(&ftpc->pp, "RETR %s", ftpc->file);
      state(conn, FTP_RETR);
    }
    break;
  }

  return result;
}

static CURLcode ftp_state_stor_resp(struct connectdata *conn,
                                    int ftpcode, ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;

  if(ftpcode >= 400) {
    failf(data, "Failed FTP upload: %0d", ftpcode);
    state(conn, FTP_STOP);
    /* oops, we never close the sockets! */
    return CURLE_UPLOAD_FAILED;
  }

  conn->proto.ftpc.state_saved = instate;

  /* PORT means we are now awaiting the server to connect to us. */
  if(data->set.ftp_use_port) {
    bool connected;

    state(conn, FTP_STOP); /* no longer in STOR state */

    result = AllowServerConnect(conn, &connected);
    if(result)
      return result;

    if(!connected) {
      struct ftp_conn *ftpc = &conn->proto.ftpc;
      infof(data, "Data conn was not available immediately\n");
      ftpc->wait_data_conn = TRUE;
    }

    return CURLE_OK;
  }
  return InitiateTransfer(conn);
}

/* for LIST and RETR responses */
static CURLcode ftp_state_get_resp(struct connectdata *conn,
                                    int ftpcode,
                                    ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;

  if((ftpcode == 150) || (ftpcode == 125)) {

    /*
      A;
      150 Opening BINARY mode data connection for /etc/passwd (2241
      bytes).  (ok, the file is being transferred)

      B:
      150 Opening ASCII mode data connection for /bin/ls

      C:
      150 ASCII data connection for /bin/ls (137.167.104.91,37445) (0 bytes).

      D:
      150 Opening ASCII mode data connection for [file] (0.0.0.0,0) (545 bytes)

      E:
      125 Data connection already open; Transfer starting. */

    curl_off_t size = -1; /* default unknown size */


    /*
     * It appears that there are FTP-servers that return size 0 for files when
     * SIZE is used on the file while being in BINARY mode. To work around
     * that (stupid) behavior, we attempt to parse the RETR response even if
     * the SIZE returned size zero.
     *
     * Debugging help from Salvatore Sorrentino on February 26, 2003.
     */

    if((instate != FTP_LIST) &&
       !data->set.prefer_ascii &&
       (ftp->downloadsize < 1)) {
      /*
       * It seems directory listings either don't show the size or very
       * often uses size 0 anyway. ASCII transfers may very well turn out
       * that the transferred amount of data is not the same as this line
       * tells, why using this number in those cases only confuses us.
       *
       * Example D above makes this parsing a little tricky */
      char *bytes;
      char *buf = data->state.buffer;
      bytes = strstr(buf, " bytes");
      if(bytes) {
        long in = (long)(--bytes-buf);
        /* this is a hint there is size information in there! ;-) */
        while(--in) {
          /* scan for the left parenthesis and break there */
          if('(' == *bytes)
            break;
          /* skip only digits */
          if(!ISDIGIT(*bytes)) {
            bytes = NULL;
            break;
          }
          /* one more estep backwards */
          bytes--;
        }
        /* if we have nothing but digits: */
        if(bytes++) {
          /* get the number! */
          (void)curlx_strtoofft(bytes, NULL, 0, &size);
        }
      }
    }
    else if(ftp->downloadsize > -1)
      size = ftp->downloadsize;

    if(size > data->req.maxdownload && data->req.maxdownload > 0)
      size = data->req.size = data->req.maxdownload;
    else if((instate != FTP_LIST) && (data->set.prefer_ascii))
      size = -1; /* kludge for servers that understate ASCII mode file size */

    infof(data, "Maxdownload = %" CURL_FORMAT_CURL_OFF_T "\n",
          data->req.maxdownload);

    if(instate != FTP_LIST)
      infof(data, "Getting file with size: %" CURL_FORMAT_CURL_OFF_T "\n",
            size);

    /* FTP download: */
    conn->proto.ftpc.state_saved = instate;
    conn->proto.ftpc.retr_size_saved = size;

    if(data->set.ftp_use_port) {
      bool connected;

      result = AllowServerConnect(conn, &connected);
      if(result)
        return result;

      if(!connected) {
        struct ftp_conn *ftpc = &conn->proto.ftpc;
        infof(data, "Data conn was not available immediately\n");
        state(conn, FTP_STOP);
        ftpc->wait_data_conn = TRUE;
      }
    }
    else
      return InitiateTransfer(conn);
  }
  else {
    if((instate == FTP_LIST) && (ftpcode == 450)) {
      /* simply no matching files in the dir listing */
      ftp->transfer = FTPTRANSFER_NONE; /* don't download anything */
      state(conn, FTP_STOP); /* this phase is over */
    }
    else {
      failf(data, "RETR response: %03d", ftpcode);
      return instate == FTP_RETR && ftpcode == 550?
        CURLE_REMOTE_FILE_NOT_FOUND:
        CURLE_FTP_COULDNT_RETR_FILE;
    }
  }

  return result;
}

/* after USER, PASS and ACCT */
static CURLcode ftp_state_loggedin(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;

  if(conn->ssl[FIRSTSOCKET].use) {
    /* PBSZ = PROTECTION BUFFER SIZE.

    The 'draft-murray-auth-ftp-ssl' (draft 12, page 7) says:

    Specifically, the PROT command MUST be preceded by a PBSZ
    command and a PBSZ command MUST be preceded by a successful
    security data exchange (the TLS negotiation in this case)

    ... (and on page 8):

    Thus the PBSZ command must still be issued, but must have a
    parameter of '0' to indicate that no buffering is taking place
    and the data connection should not be encapsulated.
    */
    PPSENDF(&conn->proto.ftpc.pp, "PBSZ %d", 0);
    state(conn, FTP_PBSZ);
  }
  else {
    result = ftp_state_pwd(conn);
  }
  return result;
}

/* for USER and PASS responses */
static CURLcode ftp_state_user_resp(struct connectdata *conn,
                                    int ftpcode,
                                    ftpstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  (void)instate; /* no use for this yet */

  /* some need password anyway, and others just return 2xx ignored */
  if((ftpcode == 331) && (ftpc->state == FTP_USER)) {
    /* 331 Password required for ...
       (the server requires to send the user's password too) */
    PPSENDF(&ftpc->pp, "PASS %s", ftp->passwd?ftp->passwd:"");
    state(conn, FTP_PASS);
  }
  else if(ftpcode/100 == 2) {
    /* 230 User ... logged in.
       (the user logged in with or without password) */
    result = ftp_state_loggedin(conn);
  }
  else if(ftpcode == 332) {
    if(data->set.str[STRING_FTP_ACCOUNT]) {
      PPSENDF(&ftpc->pp, "ACCT %s", data->set.str[STRING_FTP_ACCOUNT]);
      state(conn, FTP_ACCT);
    }
    else {
      failf(data, "ACCT requested but none available");
      result = CURLE_LOGIN_DENIED;
    }
  }
  else {
    /* All other response codes, like:

    530 User ... access denied
    (the server denies to log the specified user) */

    if(conn->data->set.str[STRING_FTP_ALTERNATIVE_TO_USER] &&
        !conn->data->state.ftp_trying_alternative) {
      /* Ok, USER failed.  Let's try the supplied command. */
      PPSENDF(&conn->proto.ftpc.pp, "%s",
              conn->data->set.str[STRING_FTP_ALTERNATIVE_TO_USER]);
      conn->data->state.ftp_trying_alternative = TRUE;
      state(conn, FTP_USER);
      result = CURLE_OK;
    }
    else {
      failf(data, "Access denied: %03d", ftpcode);
      result = CURLE_LOGIN_DENIED;
    }
  }
  return result;
}

/* for ACCT response */
static CURLcode ftp_state_acct_resp(struct connectdata *conn,
                                    int ftpcode)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  if(ftpcode != 230) {
    failf(data, "ACCT rejected by server: %03d", ftpcode);
    result = CURLE_FTP_WEIRD_PASS_REPLY; /* FIX */
  }
  else
    result = ftp_state_loggedin(conn);

  return result;
}


static CURLcode ftp_statemach_act(struct connectdata *conn)
{
  CURLcode result;
  curl_socket_t sock = conn->sock[FIRSTSOCKET];
  struct Curl_easy *data = conn->data;
  int ftpcode;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;
  static const char ftpauth[][4]  = { "SSL", "TLS" };
  size_t nread = 0;

  if(pp->sendleft)
    return Curl_pp_flushsend(pp);

  result = ftp_readresp(sock, pp, &ftpcode, &nread);
  if(result)
    return result;

  if(ftpcode) {
    /* we have now received a full FTP server response */
    switch(ftpc->state) {
    case FTP_WAIT220:
      if(ftpcode == 230)
        /* 230 User logged in - already! */
        return ftp_state_user_resp(conn, ftpcode, ftpc->state);
      else if(ftpcode != 220) {
        failf(data, "Got a %03d ftp-server response when 220 was expected",
              ftpcode);
        return CURLE_WEIRD_SERVER_REPLY;
      }

      /* We have received a 220 response fine, now we proceed. */
#ifdef HAVE_GSSAPI
      if(data->set.krb) {
        /* If not anonymous login, try a secure login. Note that this
           procedure is still BLOCKING. */

        Curl_sec_request_prot(conn, "private");
        /* We set private first as default, in case the line below fails to
           set a valid level */
        Curl_sec_request_prot(conn, data->set.str[STRING_KRB_LEVEL]);

        if(Curl_sec_login(conn))
          infof(data, "Logging in with password in cleartext!\n");
        else
          infof(data, "Authentication successful\n");
      }
#endif

      if(data->set.use_ssl &&
         (!conn->ssl[FIRSTSOCKET].use ||
          (conn->bits.proxy_ssl_connected[FIRSTSOCKET] &&
           !conn->proxy_ssl[FIRSTSOCKET].use))) {
        /* We don't have a SSL/TLS connection yet, but FTPS is
           requested. Try a FTPS connection now */

        ftpc->count3 = 0;
        switch(data->set.ftpsslauth) {
        case CURLFTPAUTH_DEFAULT:
        case CURLFTPAUTH_SSL:
          ftpc->count2 = 1; /* add one to get next */
          ftpc->count1 = 0;
          break;
        case CURLFTPAUTH_TLS:
          ftpc->count2 = -1; /* subtract one to get next */
          ftpc->count1 = 1;
          break;
        default:
          failf(data, "unsupported parameter to CURLOPT_FTPSSLAUTH: %d",
                (int)data->set.ftpsslauth);
          return CURLE_UNKNOWN_OPTION; /* we don't know what to do */
        }
        PPSENDF(&ftpc->pp, "AUTH %s", ftpauth[ftpc->count1]);
        state(conn, FTP_AUTH);
      }
      else {
        result = ftp_state_user(conn);
        if(result)
          return result;
      }

      break;

    case FTP_AUTH:
      /* we have gotten the response to a previous AUTH command */

      /* RFC2228 (page 5) says:
       *
       * If the server is willing to accept the named security mechanism,
       * and does not require any security data, it must respond with
       * reply code 234/334.
       */

      if((ftpcode == 234) || (ftpcode == 334)) {
        /* Curl_ssl_connect is BLOCKING */
        result = Curl_ssl_connect(conn, FIRSTSOCKET);
        if(!result) {
          conn->bits.ftp_use_data_ssl = FALSE; /* clear-text data */
          result = ftp_state_user(conn);
        }
      }
      else if(ftpc->count3 < 1) {
        ftpc->count3++;
        ftpc->count1 += ftpc->count2; /* get next attempt */
        result = Curl_pp_sendf(&ftpc->pp, "AUTH %s", ftpauth[ftpc->count1]);
        /* remain in this same state */
      }
      else {
        if(data->set.use_ssl > CURLUSESSL_TRY)
          /* we failed and CURLUSESSL_CONTROL or CURLUSESSL_ALL is set */
          result = CURLE_USE_SSL_FAILED;
        else
          /* ignore the failure and continue */
          result = ftp_state_user(conn);
      }

      if(result)
        return result;
      break;

    case FTP_USER:
    case FTP_PASS:
      result = ftp_state_user_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_ACCT:
      result = ftp_state_acct_resp(conn, ftpcode);
      break;

    case FTP_PBSZ:
      PPSENDF(&ftpc->pp, "PROT %c",
              data->set.use_ssl == CURLUSESSL_CONTROL ? 'C' : 'P');
      state(conn, FTP_PROT);

      break;

    case FTP_PROT:
      if(ftpcode/100 == 2)
        /* We have enabled SSL for the data connection! */
        conn->bits.ftp_use_data_ssl =
          (data->set.use_ssl != CURLUSESSL_CONTROL) ? TRUE : FALSE;
      /* FTP servers typically responds with 500 if they decide to reject
         our 'P' request */
      else if(data->set.use_ssl > CURLUSESSL_CONTROL)
        /* we failed and bails out */
        return CURLE_USE_SSL_FAILED;

      if(data->set.ftp_ccc) {
        /* CCC - Clear Command Channel
         */
        PPSENDF(&ftpc->pp, "%s", "CCC");
        state(conn, FTP_CCC);
      }
      else {
        result = ftp_state_pwd(conn);
        if(result)
          return result;
      }
      break;

    case FTP_CCC:
      if(ftpcode < 500) {
        /* First shut down the SSL layer (note: this call will block) */
        result = Curl_ssl_shutdown(conn, FIRSTSOCKET);

        if(result) {
          failf(conn->data, "Failed to clear the command channel (CCC)");
          return result;
        }
      }

      /* Then continue as normal */
      result = ftp_state_pwd(conn);
      if(result)
        return result;
      break;

    case FTP_PWD:
      if(ftpcode == 257) {
        char *ptr = &data->state.buffer[4];  /* start on the first letter */
        const size_t buf_size = data->set.buffer_size;
        char *dir;
        bool entry_extracted = FALSE;

        dir = malloc(nread + 1);
        if(!dir)
          return CURLE_OUT_OF_MEMORY;

        /* Reply format is like
           257<space>[rubbish]"<directory-name>"<space><commentary> and the
           RFC959 says

           The directory name can contain any character; embedded
           double-quotes should be escaped by double-quotes (the
           "quote-doubling" convention).
        */

        /* scan for the first double-quote for non-standard responses */
        while(ptr < &data->state.buffer[buf_size]
              && *ptr != '\n' && *ptr != '\0' && *ptr != '"')
          ptr++;

        if('\"' == *ptr) {
          /* it started good */
          char *store;
          ptr++;
          for(store = dir; *ptr;) {
            if('\"' == *ptr) {
              if('\"' == ptr[1]) {
                /* "quote-doubling" */
                *store = ptr[1];
                ptr++;
              }
              else {
                /* end of path */
                entry_extracted = TRUE;
                break; /* get out of this loop */
              }
            }
            else
              *store = *ptr;
            store++;
            ptr++;
          }
          *store = '\0'; /* zero terminate */
        }
        if(entry_extracted) {
          /* If the path name does not look like an absolute path (i.e.: it
             does not start with a '/'), we probably need some server-dependent
             adjustments. For example, this is the case when connecting to
             an OS400 FTP server: this server supports two name syntaxes,
             the default one being incompatible with standard paths. In
             addition, this server switches automatically to the regular path
             syntax when one is encountered in a command: this results in
             having an entrypath in the wrong syntax when later used in CWD.
               The method used here is to check the server OS: we do it only
             if the path name looks strange to minimize overhead on other
             systems. */

          if(!ftpc->server_os && dir[0] != '/') {

            result = Curl_pp_sendf(&ftpc->pp, "%s", "SYST");
            if(result) {
              free(dir);
              return result;
            }
            Curl_safefree(ftpc->entrypath);
            ftpc->entrypath = dir; /* remember this */
            infof(data, "Entry path is '%s'\n", ftpc->entrypath);
            /* also save it where getinfo can access it: */
            data->state.most_recent_ftp_entrypath = ftpc->entrypath;
            state(conn, FTP_SYST);
            break;
          }

          Curl_safefree(ftpc->entrypath);
          ftpc->entrypath = dir; /* remember this */
          infof(data, "Entry path is '%s'\n", ftpc->entrypath);
          /* also save it where getinfo can access it: */
          data->state.most_recent_ftp_entrypath = ftpc->entrypath;
        }
        else {
          /* couldn't get the path */
          free(dir);
          infof(data, "Failed to figure out path\n");
        }
      }
      state(conn, FTP_STOP); /* we are done with the CONNECT phase! */
      DEBUGF(infof(data, "protocol connect phase DONE\n"));
      break;

    case FTP_SYST:
      if(ftpcode == 215) {
        char *ptr = &data->state.buffer[4];  /* start on the first letter */
        char *os;
        char *store;

        os = malloc(nread + 1);
        if(!os)
          return CURLE_OUT_OF_MEMORY;

        /* Reply format is like
           215<space><OS-name><space><commentary>
        */
        while(*ptr == ' ')
          ptr++;
        for(store = os; *ptr && *ptr != ' ';)
          *store++ = *ptr++;
        *store = '\0'; /* zero terminate */

        /* Check for special servers here. */

        if(strcasecompare(os, "OS/400")) {
          /* Force OS400 name format 1. */
          result = Curl_pp_sendf(&ftpc->pp, "%s", "SITE NAMEFMT 1");
          if(result) {
            free(os);
            return result;
          }
          /* remember target server OS */
          Curl_safefree(ftpc->server_os);
          ftpc->server_os = os;
          state(conn, FTP_NAMEFMT);
          break;
        }
        /* Nothing special for the target server. */
        /* remember target server OS */
        Curl_safefree(ftpc->server_os);
        ftpc->server_os = os;
      }
      else {
        /* Cannot identify server OS. Continue anyway and cross fingers. */
      }

      state(conn, FTP_STOP); /* we are done with the CONNECT phase! */
      DEBUGF(infof(data, "protocol connect phase DONE\n"));
      break;

    case FTP_NAMEFMT:
      if(ftpcode == 250) {
        /* Name format change successful: reload initial path. */
        ftp_state_pwd(conn);
        break;
      }

      state(conn, FTP_STOP); /* we are done with the CONNECT phase! */
      DEBUGF(infof(data, "protocol connect phase DONE\n"));
      break;

    case FTP_QUOTE:
    case FTP_POSTQUOTE:
    case FTP_RETR_PREQUOTE:
    case FTP_STOR_PREQUOTE:
      if((ftpcode >= 400) && !ftpc->count2) {
        /* failure response code, and not allowed to fail */
        failf(conn->data, "QUOT command failed with %03d", ftpcode);
        return CURLE_QUOTE_ERROR;
      }
      result = ftp_state_quote(conn, FALSE, ftpc->state);
      if(result)
        return result;

      break;

    case FTP_CWD:
      if(ftpcode/100 != 2) {
        /* failure to CWD there */
        if(conn->data->set.ftp_create_missing_dirs &&
           ftpc->cwdcount && !ftpc->count2) {
          /* try making it */
          ftpc->count2++; /* counter to prevent CWD-MKD loops */
          PPSENDF(&ftpc->pp, "MKD %s", ftpc->dirs[ftpc->cwdcount - 1]);
          state(conn, FTP_MKD);
        }
        else {
          /* return failure */
          failf(data, "Server denied you to change to the given directory");
          ftpc->cwdfail = TRUE; /* don't remember this path as we failed
                                   to enter it */
          return CURLE_REMOTE_ACCESS_DENIED;
        }
      }
      else {
        /* success */
        ftpc->count2 = 0;
        if(++ftpc->cwdcount <= ftpc->dirdepth) {
          /* send next CWD */
          PPSENDF(&ftpc->pp, "CWD %s", ftpc->dirs[ftpc->cwdcount - 1]);
        }
        else {
          result = ftp_state_mdtm(conn);
          if(result)
            return result;
        }
      }
      break;

    case FTP_MKD:
      if((ftpcode/100 != 2) && !ftpc->count3--) {
        /* failure to MKD the dir */
        failf(data, "Failed to MKD dir: %03d", ftpcode);
        return CURLE_REMOTE_ACCESS_DENIED;
      }
      state(conn, FTP_CWD);
      /* send CWD */
      PPSENDF(&ftpc->pp, "CWD %s", ftpc->dirs[ftpc->cwdcount - 1]);
      break;

    case FTP_MDTM:
      result = ftp_state_mdtm_resp(conn, ftpcode);
      break;

    case FTP_TYPE:
    case FTP_LIST_TYPE:
    case FTP_RETR_TYPE:
    case FTP_STOR_TYPE:
      result = ftp_state_type_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_SIZE:
    case FTP_RETR_SIZE:
    case FTP_STOR_SIZE:
      result = ftp_state_size_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_REST:
    case FTP_RETR_REST:
      result = ftp_state_rest_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_PRET:
      if(ftpcode != 200) {
        /* there only is this one standard OK return code. */
        failf(data, "PRET command not accepted: %03d", ftpcode);
        return CURLE_FTP_PRET_FAILED;
      }
      result = ftp_state_use_pasv(conn);
      break;

    case FTP_PASV:
      result = ftp_state_pasv_resp(conn, ftpcode);
      break;

    case FTP_PORT:
      result = ftp_state_port_resp(conn, ftpcode);
      break;

    case FTP_LIST:
    case FTP_RETR:
      result = ftp_state_get_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_STOR:
      result = ftp_state_stor_resp(conn, ftpcode, ftpc->state);
      break;

    case FTP_QUIT:
      /* fallthrough, just stop! */
    default:
      /* internal error */
      state(conn, FTP_STOP);
      break;
    }
  } /* if(ftpcode) */

  return result;
}


/* called repeatedly until done from multi.c */
static CURLcode ftp_multi_statemach(struct connectdata *conn,
                                    bool *done)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  CURLcode result = Curl_pp_statemach(&ftpc->pp, FALSE, FALSE);

  /* Check for the state outside of the Curl_socket_check() return code checks
     since at times we are in fact already in this state when this function
     gets called. */
  *done = (ftpc->state == FTP_STOP) ? TRUE : FALSE;

  return result;
}

static CURLcode ftp_block_statemach(struct connectdata *conn)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;
  CURLcode result = CURLE_OK;

  while(ftpc->state != FTP_STOP) {
    result = Curl_pp_statemach(pp, TRUE, TRUE /* disconnecting */);
    if(result)
      break;
  }

  return result;
}

/*
 * ftp_connect() should do everything that is to be considered a part of
 * the connection phase.
 *
 * The variable 'done' points to will be TRUE if the protocol-layer connect
 * phase is done when this function returns, or FALSE if not.
 *
 */
static CURLcode ftp_connect(struct connectdata *conn,
                                 bool *done) /* see description above */
{
  CURLcode result;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;

  *done = FALSE; /* default to not done yet */

  /* We always support persistent connections on ftp */
  connkeep(conn, "FTP default");

  pp->response_time = RESP_TIMEOUT; /* set default response time-out */
  pp->statemach_act = ftp_statemach_act;
  pp->endofresp = ftp_endofresp;
  pp->conn = conn;

  if(conn->handler->flags & PROTOPT_SSL) {
    /* BLOCKING */
    result = Curl_ssl_connect(conn, FIRSTSOCKET);
    if(result)
      return result;
  }

  Curl_pp_init(pp); /* init the generic pingpong data */

  /* When we connect, we start in the state where we await the 220
     response */
  state(conn, FTP_WAIT220);

  result = ftp_multi_statemach(conn, done);

  return result;
}

/***********************************************************************
 *
 * ftp_done()
 *
 * The DONE function. This does what needs to be done after a single DO has
 * performed.
 *
 * Input argument is already checked for validity.
 */
static CURLcode ftp_done(struct connectdata *conn, CURLcode status,
                         bool premature)
{
  struct Curl_easy *data = conn->data;
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;
  ssize_t nread;
  int ftpcode;
  CURLcode result = CURLE_OK;
  char *path = NULL;

  if(!ftp)
    return CURLE_OK;

  switch(status) {
  case CURLE_BAD_DOWNLOAD_RESUME:
  case CURLE_FTP_WEIRD_PASV_REPLY:
  case CURLE_FTP_PORT_FAILED:
  case CURLE_FTP_ACCEPT_FAILED:
  case CURLE_FTP_ACCEPT_TIMEOUT:
  case CURLE_FTP_COULDNT_SET_TYPE:
  case CURLE_FTP_COULDNT_RETR_FILE:
  case CURLE_PARTIAL_FILE:
  case CURLE_UPLOAD_FAILED:
  case CURLE_REMOTE_ACCESS_DENIED:
  case CURLE_FILESIZE_EXCEEDED:
  case CURLE_REMOTE_FILE_NOT_FOUND:
  case CURLE_WRITE_ERROR:
    /* the connection stays alive fine even though this happened */
    /* fall-through */
  case CURLE_OK: /* doesn't affect the control connection's status */
    if(!premature)
      break;

    /* until we cope better with prematurely ended requests, let them
     * fallback as if in complete failure */
    /* FALLTHROUGH */
  default:       /* by default, an error means the control connection is
                    wedged and should not be used anymore */
    ftpc->ctl_valid = FALSE;
    ftpc->cwdfail = TRUE; /* set this TRUE to prevent us to remember the
                             current path, as this connection is going */
    connclose(conn, "FTP ended with bad error code");
    result = status;      /* use the already set error code */
    break;
  }

  /* now store a copy of the directory we are in */
  free(ftpc->prevpath);

  if(data->state.wildcardmatch) {
    if(data->set.chunk_end && ftpc->file) {
      Curl_set_in_callback(data, true);
      data->set.chunk_end(data->wildcard.customptr);
      Curl_set_in_callback(data, false);
    }
    ftpc->known_filesize = -1;
  }

  if(!result)
    /* get the "raw" path */
    result = Curl_urldecode(data, ftp->path, 0, &path, NULL, TRUE);
  if(result) {
    /* We can limp along anyway (and should try to since we may already be in
     * the error path) */
    ftpc->ctl_valid = FALSE; /* mark control connection as bad */
    connclose(conn, "FTP: out of memory!"); /* mark for connection closure */
    ftpc->prevpath = NULL; /* no path remembering */
  }
  else {
    size_t flen = ftpc->file?strlen(ftpc->file):0; /* file is "raw" already */
    size_t dlen = strlen(path)-flen;
    if(!ftpc->cwdfail) {
      ftpc->prevmethod = data->set.ftp_filemethod;
      if(dlen && (data->set.ftp_filemethod != FTPFILE_NOCWD)) {
        ftpc->prevpath = path;
        if(flen)
          /* if 'path' is not the whole string */
          ftpc->prevpath[dlen] = 0; /* terminate */
      }
      else {
        free(path);
        /* we never changed dir */
        ftpc->prevpath = strdup("");
        if(!ftpc->prevpath)
          return CURLE_OUT_OF_MEMORY;
      }
      if(ftpc->prevpath)
        infof(data, "Remembering we are in dir \"%s\"\n", ftpc->prevpath);
    }
    else {
      ftpc->prevpath = NULL; /* no path */
      free(path);
    }
  }
  /* free the dir tree and file parts */
  freedirs(ftpc);

  /* shut down the socket to inform the server we're done */

#ifdef _WIN32_WCE
  shutdown(conn->sock[SECONDARYSOCKET], 2);  /* SD_BOTH */
#endif

  if(conn->sock[SECONDARYSOCKET] != CURL_SOCKET_BAD) {
    if(!result && ftpc->dont_check && data->req.maxdownload > 0) {
      /* partial download completed */
      result = Curl_pp_sendf(pp, "%s", "ABOR");
      if(result) {
        failf(data, "Failure sending ABOR command: %s",
              curl_easy_strerror(result));
        ftpc->ctl_valid = FALSE; /* mark control connection as bad */
        connclose(conn, "ABOR command failed"); /* connection closure */
      }
    }

    if(conn->ssl[SECONDARYSOCKET].use) {
      /* The secondary socket is using SSL so we must close down that part
         first before we close the socket for real */
      Curl_ssl_close(conn, SECONDARYSOCKET);

      /* Note that we keep "use" set to TRUE since that (next) connection is
         still requested to use SSL */
    }
    close_secondarysocket(conn);
  }

  if(!result && (ftp->transfer == FTPTRANSFER_BODY) && ftpc->ctl_valid &&
     pp->pending_resp && !premature) {
    /*
     * Let's see what the server says about the transfer we just performed,
     * but lower the timeout as sometimes this connection has died while the
     * data has been transferred. This happens when doing through NATs etc that
     * abandon old silent connections.
     */
    long old_time = pp->response_time;

    pp->response_time = 60*1000; /* give it only a minute for now */
    pp->response = Curl_now(); /* timeout relative now */

    result = Curl_GetFTPResponse(&nread, conn, &ftpcode);

    pp->response_time = old_time; /* set this back to previous value */

    if(!nread && (CURLE_OPERATION_TIMEDOUT == result)) {
      failf(data, "control connection looks dead");
      ftpc->ctl_valid = FALSE; /* mark control connection as bad */
      connclose(conn, "Timeout or similar in FTP DONE operation"); /* close */
    }

    if(result)
      return result;

    if(ftpc->dont_check && data->req.maxdownload > 0) {
      /* we have just sent ABOR and there is no reliable way to check if it was
       * successful or not; we have to close the connection now */
      infof(data, "partial download completed, closing connection\n");
      connclose(conn, "Partial download with no ability to check");
      return result;
    }

    if(!ftpc->dont_check) {
      /* 226 Transfer complete, 250 Requested file action okay, completed. */
      if((ftpcode != 226) && (ftpcode != 250)) {
        failf(data, "server did not report OK, got %d", ftpcode);
        result = CURLE_PARTIAL_FILE;
      }
    }
  }

  if(result || premature)
    /* the response code from the transfer showed an error already so no
       use checking further */
    ;
  else if(data->set.upload) {
    if((-1 != data->state.infilesize) &&
       (data->state.infilesize != data->req.writebytecount) &&
       !data->set.crlf &&
       (ftp->transfer == FTPTRANSFER_BODY)) {
      failf(data, "Uploaded unaligned file size (%" CURL_FORMAT_CURL_OFF_T
            " out of %" CURL_FORMAT_CURL_OFF_T " bytes)",
            data->req.bytecount, data->state.infilesize);
      result = CURLE_PARTIAL_FILE;
    }
  }
  else {
    if((-1 != data->req.size) &&
       (data->req.size != data->req.bytecount) &&
#ifdef CURL_DO_LINEEND_CONV
       /* Most FTP servers don't adjust their file SIZE response for CRLFs, so
        * we'll check to see if the discrepancy can be explained by the number
        * of CRLFs we've changed to LFs.
        */
       ((data->req.size + data->state.crlf_conversions) !=
        data->req.bytecount) &&
#endif /* CURL_DO_LINEEND_CONV */
       (data->req.maxdownload != data->req.bytecount)) {
      failf(data, "Received only partial file: %" CURL_FORMAT_CURL_OFF_T
            " bytes", data->req.bytecount);
      result = CURLE_PARTIAL_FILE;
    }
    else if(!ftpc->dont_check &&
            !data->req.bytecount &&
            (data->req.size>0)) {
      failf(data, "No data was received!");
      result = CURLE_FTP_COULDNT_RETR_FILE;
    }
  }

  /* clear these for next connection */
  ftp->transfer = FTPTRANSFER_BODY;
  ftpc->dont_check = FALSE;

  /* Send any post-transfer QUOTE strings? */
  if(!status && !result && !premature && data->set.postquote)
    result = ftp_sendquote(conn, data->set.postquote);
  Curl_safefree(ftp->pathalloc);
  return result;
}

/***********************************************************************
 *
 * ftp_sendquote()
 *
 * Where a 'quote' means a list of custom commands to send to the server.
 * The quote list is passed as an argument.
 *
 * BLOCKING
 */

static
CURLcode ftp_sendquote(struct connectdata *conn, struct curl_slist *quote)
{
  struct curl_slist *item;
  ssize_t nread;
  int ftpcode;
  CURLcode result;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;

  item = quote;
  while(item) {
    if(item->data) {
      char *cmd = item->data;
      bool acceptfail = FALSE;

      /* if a command starts with an asterisk, which a legal FTP command never
         can, the command will be allowed to fail without it causing any
         aborts or cancels etc. It will cause libcurl to act as if the command
         is successful, whatever the server reponds. */

      if(cmd[0] == '*') {
        cmd++;
        acceptfail = TRUE;
      }

      PPSENDF(&conn->proto.ftpc.pp, "%s", cmd);

      pp->response = Curl_now(); /* timeout relative now */

      result = Curl_GetFTPResponse(&nread, conn, &ftpcode);
      if(result)
        return result;

      if(!acceptfail && (ftpcode >= 400)) {
        failf(conn->data, "QUOT string not accepted: %s", cmd);
        return CURLE_QUOTE_ERROR;
      }
    }

    item = item->next;
  }

  return CURLE_OK;
}

/***********************************************************************
 *
 * ftp_need_type()
 *
 * Returns TRUE if we in the current situation should send TYPE
 */
static int ftp_need_type(struct connectdata *conn,
                         bool ascii_wanted)
{
  return conn->proto.ftpc.transfertype != (ascii_wanted?'A':'I');
}

/***********************************************************************
 *
 * ftp_nb_type()
 *
 * Set TYPE. We only deal with ASCII or BINARY so this function
 * sets one of them.
 * If the transfer type is not sent, simulate on OK response in newstate
 */
static CURLcode ftp_nb_type(struct connectdata *conn,
                            bool ascii, ftpstate newstate)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  CURLcode result;
  char want = (char)(ascii?'A':'I');

  if(ftpc->transfertype == want) {
    state(conn, newstate);
    return ftp_state_type_resp(conn, 200, newstate);
  }

  PPSENDF(&ftpc->pp, "TYPE %c", want);
  state(conn, newstate);

  /* keep track of our current transfer type */
  ftpc->transfertype = want;
  return CURLE_OK;
}

/***************************************************************************
 *
 * ftp_pasv_verbose()
 *
 * This function only outputs some informationals about this second connection
 * when we've issued a PASV command before and thus we have connected to a
 * possibly new IP address.
 *
 */
#ifndef CURL_DISABLE_VERBOSE_STRINGS
static void
ftp_pasv_verbose(struct connectdata *conn,
                 Curl_addrinfo *ai,
                 char *newhost, /* ascii version */
                 int port)
{
  char buf[256];
  Curl_printable_address(ai, buf, sizeof(buf));
  infof(conn->data, "Connecting to %s (%s) port %d\n", newhost, buf, port);
}
#endif

/*
 * ftp_do_more()
 *
 * This function shall be called when the second FTP (data) connection is
 * connected.
 *
 * 'complete' can return 0 for incomplete, 1 for done and -1 for go back
 * (which basically is only for when PASV is being sent to retry a failed
 * EPSV).
 */

static CURLcode ftp_do_more(struct connectdata *conn, int *completep)
{
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  CURLcode result = CURLE_OK;
  bool connected = FALSE;
  bool complete = FALSE;

  /* the ftp struct is inited in ftp_connect() */
  struct FTP *ftp = data->req.protop;

  /* if the second connection isn't done yet, wait for it */
  if(!conn->bits.tcpconnect[SECONDARYSOCKET]) {
    if(Curl_connect_ongoing(conn)) {
      /* As we're in TUNNEL_CONNECT state now, we know the proxy name and port
         aren't used so we blank their arguments. */
      result = Curl_proxyCONNECT(conn, SECONDARYSOCKET, NULL, 0);

      return result;
    }

    result = Curl_is_connected(conn, SECONDARYSOCKET, &connected);

    /* Ready to do more? */
    if(connected) {
      DEBUGF(infof(data, "DO-MORE connected phase starts\n"));
    }
    else {
      if(result && (ftpc->count1 == 0)) {
        *completep = -1; /* go back to DOING please */
        /* this is a EPSV connect failing, try PASV instead */
        return ftp_epsv_disable(conn);
      }
      return result;
    }
  }

  result = Curl_proxy_connect(conn, SECONDARYSOCKET);
  if(result)
    return result;

  if(CONNECT_SECONDARYSOCKET_PROXY_SSL())
    return result;

  if(conn->bits.tunnel_proxy && conn->bits.httpproxy &&
     Curl_connect_ongoing(conn))
    return result;


  if(ftpc->state) {
    /* already in a state so skip the initial commands.
       They are only done to kickstart the do_more state */
    result = ftp_multi_statemach(conn, &complete);

    *completep = (int)complete;

    /* if we got an error or if we don't wait for a data connection return
       immediately */
    if(result || (ftpc->wait_data_conn != TRUE))
      return result;

    if(ftpc->wait_data_conn)
      /* if we reach the end of the FTP state machine here, *complete will be
         TRUE but so is ftpc->wait_data_conn, which says we need to wait for
         the data connection and therefore we're not actually complete */
      *completep = 0;
  }

  if(ftp->transfer <= FTPTRANSFER_INFO) {
    /* a transfer is about to take place, or if not a file name was given
       so we'll do a SIZE on it later and then we need the right TYPE first */

    if(ftpc->wait_data_conn == TRUE) {
      bool serv_conned;

      result = ReceivedServerConnect(conn, &serv_conned);
      if(result)
        return result; /* Failed to accept data connection */

      if(serv_conned) {
        /* It looks data connection is established */
        result = AcceptServerConnect(conn);
        ftpc->wait_data_conn = FALSE;
        if(!result)
          result = InitiateTransfer(conn);

        if(result)
          return result;

        *completep = 1; /* this state is now complete when the server has
                           connected back to us */
      }
    }
    else if(data->set.upload) {
      result = ftp_nb_type(conn, data->set.prefer_ascii, FTP_STOR_TYPE);
      if(result)
        return result;

      result = ftp_multi_statemach(conn, &complete);
      if(ftpc->wait_data_conn)
        /* if we reach the end of the FTP state machine here, *complete will be
           TRUE but so is ftpc->wait_data_conn, which says we need to wait for
           the data connection and therefore we're not actually complete */
        *completep = 0;
      else
        *completep = (int)complete;
    }
    else {
      /* download */
      ftp->downloadsize = -1; /* unknown as of yet */

      result = Curl_range(conn);

      if(result == CURLE_OK && data->req.maxdownload >= 0) {
        /* Don't check for successful transfer */
        ftpc->dont_check = TRUE;
      }

      if(result)
        ;
      else if(data->set.ftp_list_only || !ftpc->file) {
        /* The specified path ends with a slash, and therefore we think this
           is a directory that is requested, use LIST. But before that we
           need to set ASCII transfer mode. */

        /* But only if a body transfer was requested. */
        if(ftp->transfer == FTPTRANSFER_BODY) {
          result = ftp_nb_type(conn, TRUE, FTP_LIST_TYPE);
          if(result)
            return result;
        }
        /* otherwise just fall through */
      }
      else {
        result = ftp_nb_type(conn, data->set.prefer_ascii, FTP_RETR_TYPE);
        if(result)
          return result;
      }

      result = ftp_multi_statemach(conn, &complete);
      *completep = (int)complete;
    }
    return result;
  }

  if(!result && (ftp->transfer != FTPTRANSFER_BODY))
    /* no data to transfer. FIX: it feels like a kludge to have this here
       too! */
    Curl_setup_transfer(data, -1, -1, FALSE, -1);

  if(!ftpc->wait_data_conn) {
    /* no waiting for the data connection so this is now complete */
    *completep = 1;
    DEBUGF(infof(data, "DO-MORE phase ends with %d\n", (int)result));
  }

  return result;
}



/***********************************************************************
 *
 * ftp_perform()
 *
 * This is the actual DO function for FTP. Get a file/directory according to
 * the options previously setup.
 */

static
CURLcode ftp_perform(struct connectdata *conn,
                     bool *connected,  /* connect status after PASV / PORT */
                     bool *dophase_done)
{
  /* this is FTP and no proxy */
  CURLcode result = CURLE_OK;

  DEBUGF(infof(conn->data, "DO phase starts\n"));

  if(conn->data->set.opt_no_body) {
    /* requested no body means no transfer... */
    struct FTP *ftp = conn->data->req.protop;
    ftp->transfer = FTPTRANSFER_INFO;
  }

  *dophase_done = FALSE; /* not done yet */

  /* start the first command in the DO phase */
  result = ftp_state_quote(conn, TRUE, FTP_QUOTE);
  if(result)
    return result;

  /* run the state-machine */
  result = ftp_multi_statemach(conn, dophase_done);

  *connected = conn->bits.tcpconnect[SECONDARYSOCKET];

  infof(conn->data, "ftp_perform ends with SECONDARY: %d\n", *connected);

  if(*dophase_done)
    DEBUGF(infof(conn->data, "DO phase is complete1\n"));

  return result;
}

static void wc_data_dtor(void *ptr)
{
  struct ftp_wc *ftpwc = ptr;
  if(ftpwc && ftpwc->parser)
    Curl_ftp_parselist_data_free(&ftpwc->parser);
  free(ftpwc);
}

static CURLcode init_wc_data(struct connectdata *conn)
{
  char *last_slash;
  struct FTP *ftp = conn->data->req.protop;
  char *path = ftp->path;
  struct WildcardData *wildcard = &(conn->data->wildcard);
  CURLcode result = CURLE_OK;
  struct ftp_wc *ftpwc = NULL;

  last_slash = strrchr(ftp->path, '/');
  if(last_slash) {
    last_slash++;
    if(last_slash[0] == '\0') {
      wildcard->state = CURLWC_CLEAN;
      result = ftp_parse_url_path(conn);
      return result;
    }
    wildcard->pattern = strdup(last_slash);
    if(!wildcard->pattern)
      return CURLE_OUT_OF_MEMORY;
    last_slash[0] = '\0'; /* cut file from path */
  }
  else { /* there is only 'wildcard pattern' or nothing */
    if(path[0]) {
      wildcard->pattern = strdup(path);
      if(!wildcard->pattern)
        return CURLE_OUT_OF_MEMORY;
      path[0] = '\0';
    }
    else { /* only list */
      wildcard->state = CURLWC_CLEAN;
      result = ftp_parse_url_path(conn);
      return result;
    }
  }

  /* program continues only if URL is not ending with slash, allocate needed
     resources for wildcard transfer */

  /* allocate ftp protocol specific wildcard data */
  ftpwc = calloc(1, sizeof(struct ftp_wc));
  if(!ftpwc) {
    result = CURLE_OUT_OF_MEMORY;
    goto fail;
  }

  /* INITIALIZE parselist structure */
  ftpwc->parser = Curl_ftp_parselist_data_alloc();
  if(!ftpwc->parser) {
    result = CURLE_OUT_OF_MEMORY;
    goto fail;
  }

  wildcard->protdata = ftpwc; /* put it to the WildcardData tmp pointer */
  wildcard->dtor = wc_data_dtor;

  /* wildcard does not support NOCWD option (assert it?) */
  if(conn->data->set.ftp_filemethod == FTPFILE_NOCWD)
    conn->data->set.ftp_filemethod = FTPFILE_MULTICWD;

  /* try to parse ftp url */
  result = ftp_parse_url_path(conn);
  if(result) {
    goto fail;
  }

  wildcard->path = strdup(ftp->path);
  if(!wildcard->path) {
    result = CURLE_OUT_OF_MEMORY;
    goto fail;
  }

  /* backup old write_function */
  ftpwc->backup.write_function = conn->data->set.fwrite_func;
  /* parsing write function */
  conn->data->set.fwrite_func = Curl_ftp_parselist;
  /* backup old file descriptor */
  ftpwc->backup.file_descriptor = conn->data->set.out;
  /* let the writefunc callback know what curl pointer is working with */
  conn->data->set.out = conn;

  infof(conn->data, "Wildcard - Parsing started\n");
  return CURLE_OK;

  fail:
  if(ftpwc) {
    Curl_ftp_parselist_data_free(&ftpwc->parser);
    free(ftpwc);
  }
  Curl_safefree(wildcard->pattern);
  wildcard->dtor = ZERO_NULL;
  wildcard->protdata = NULL;
  return result;
}

/* This is called recursively */
static CURLcode wc_statemach(struct connectdata *conn)
{
  struct WildcardData * const wildcard = &(conn->data->wildcard);
  CURLcode result = CURLE_OK;

  switch(wildcard->state) {
  case CURLWC_INIT:
    result = init_wc_data(conn);
    if(wildcard->state == CURLWC_CLEAN)
      /* only listing! */
      break;
    wildcard->state = result ? CURLWC_ERROR : CURLWC_MATCHING;
    break;

  case CURLWC_MATCHING: {
    /* In this state is LIST response successfully parsed, so lets restore
       previous WRITEFUNCTION callback and WRITEDATA pointer */
    struct ftp_wc *ftpwc = wildcard->protdata;
    conn->data->set.fwrite_func = ftpwc->backup.write_function;
    conn->data->set.out = ftpwc->backup.file_descriptor;
    ftpwc->backup.write_function = ZERO_NULL;
    ftpwc->backup.file_descriptor = NULL;
    wildcard->state = CURLWC_DOWNLOADING;

    if(Curl_ftp_parselist_geterror(ftpwc->parser)) {
      /* error found in LIST parsing */
      wildcard->state = CURLWC_CLEAN;
      return wc_statemach(conn);
    }
    if(wildcard->filelist.size == 0) {
      /* no corresponding file */
      wildcard->state = CURLWC_CLEAN;
      return CURLE_REMOTE_FILE_NOT_FOUND;
    }
    return wc_statemach(conn);
  }

  case CURLWC_DOWNLOADING: {
    /* filelist has at least one file, lets get first one */
    struct ftp_conn *ftpc = &conn->proto.ftpc;
    struct curl_fileinfo *finfo = wildcard->filelist.head->ptr;
    struct FTP *ftp = conn->data->req.protop;

    char *tmp_path = aprintf("%s%s", wildcard->path, finfo->filename);
    if(!tmp_path)
      return CURLE_OUT_OF_MEMORY;

    /* switch default ftp->path and tmp_path */
    free(ftp->pathalloc);
    ftp->pathalloc = ftp->path = tmp_path;

    infof(conn->data, "Wildcard - START of \"%s\"\n", finfo->filename);
    if(conn->data->set.chunk_bgn) {
      long userresponse;
      Curl_set_in_callback(conn->data, true);
      userresponse = conn->data->set.chunk_bgn(
        finfo, wildcard->customptr, (int)wildcard->filelist.size);
      Curl_set_in_callback(conn->data, false);
      switch(userresponse) {
      case CURL_CHUNK_BGN_FUNC_SKIP:
        infof(conn->data, "Wildcard - \"%s\" skipped by user\n",
              finfo->filename);
        wildcard->state = CURLWC_SKIP;
        return wc_statemach(conn);
      case CURL_CHUNK_BGN_FUNC_FAIL:
        return CURLE_CHUNK_FAILED;
      }
    }

    if(finfo->filetype != CURLFILETYPE_FILE) {
      wildcard->state = CURLWC_SKIP;
      return wc_statemach(conn);
    }

    if(finfo->flags & CURLFINFOFLAG_KNOWN_SIZE)
      ftpc->known_filesize = finfo->size;

    result = ftp_parse_url_path(conn);
    if(result)
      return result;

    /* we don't need the Curl_fileinfo of first file anymore */
    Curl_llist_remove(&wildcard->filelist, wildcard->filelist.head, NULL);

    if(wildcard->filelist.size == 0) { /* remains only one file to down. */
      wildcard->state = CURLWC_CLEAN;
      /* after that will be ftp_do called once again and no transfer
         will be done because of CURLWC_CLEAN state */
      return CURLE_OK;
    }
  } break;

  case CURLWC_SKIP: {
    if(conn->data->set.chunk_end) {
      Curl_set_in_callback(conn->data, true);
      conn->data->set.chunk_end(conn->data->wildcard.customptr);
      Curl_set_in_callback(conn->data, false);
    }
    Curl_llist_remove(&wildcard->filelist, wildcard->filelist.head, NULL);
    wildcard->state = (wildcard->filelist.size == 0) ?
                      CURLWC_CLEAN : CURLWC_DOWNLOADING;
    return wc_statemach(conn);
  }

  case CURLWC_CLEAN: {
    struct ftp_wc *ftpwc = wildcard->protdata;
    result = CURLE_OK;
    if(ftpwc)
      result = Curl_ftp_parselist_geterror(ftpwc->parser);

    wildcard->state = result ? CURLWC_ERROR : CURLWC_DONE;
  } break;

  case CURLWC_DONE:
  case CURLWC_ERROR:
  case CURLWC_CLEAR:
    if(wildcard->dtor)
      wildcard->dtor(wildcard->protdata);
    break;
  }

  return result;
}

/***********************************************************************
 *
 * ftp_do()
 *
 * This function is registered as 'curl_do' function. It decodes the path
 * parts etc as a wrapper to the actual DO function (ftp_perform).
 *
 * The input argument is already checked for validity.
 */
static CURLcode ftp_do(struct connectdata *conn, bool *done)
{
  CURLcode result = CURLE_OK;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  *done = FALSE; /* default to false */
  ftpc->wait_data_conn = FALSE; /* default to no such wait */

  if(conn->data->state.wildcardmatch) {
    result = wc_statemach(conn);
    if(conn->data->wildcard.state == CURLWC_SKIP ||
      conn->data->wildcard.state == CURLWC_DONE) {
      /* do not call ftp_regular_transfer */
      return CURLE_OK;
    }
    if(result) /* error, loop or skipping the file */
      return result;
  }
  else { /* no wildcard FSM needed */
    result = ftp_parse_url_path(conn);
    if(result)
      return result;
  }

  result = ftp_regular_transfer(conn, done);

  return result;
}


CURLcode Curl_ftpsend(struct connectdata *conn, const char *cmd)
{
  ssize_t bytes_written;
#define SBUF_SIZE 1024
  char s[SBUF_SIZE];
  size_t write_len;
  char *sptr = s;
  CURLcode result = CURLE_OK;
#ifdef HAVE_GSSAPI
  enum protection_level data_sec = conn->data_prot;
#endif

  if(!cmd)
    return CURLE_BAD_FUNCTION_ARGUMENT;

  write_len = strlen(cmd);
  if(!write_len || write_len > (sizeof(s) -3))
    return CURLE_BAD_FUNCTION_ARGUMENT;

  memcpy(&s, cmd, write_len);
  strcpy(&s[write_len], "\r\n"); /* append a trailing CRLF */
  write_len += 2;
  bytes_written = 0;

  result = Curl_convert_to_network(conn->data, s, write_len);
  /* Curl_convert_to_network calls failf if unsuccessful */
  if(result)
    return result;

  for(;;) {
#ifdef HAVE_GSSAPI
    conn->data_prot = PROT_CMD;
#endif
    result = Curl_write(conn, conn->sock[FIRSTSOCKET], sptr, write_len,
                        &bytes_written);
#ifdef HAVE_GSSAPI
    DEBUGASSERT(data_sec > PROT_NONE && data_sec < PROT_LAST);
    conn->data_prot = data_sec;
#endif

    if(result)
      break;

    if(conn->data->set.verbose)
      Curl_debug(conn->data, CURLINFO_HEADER_OUT, sptr, (size_t)bytes_written);

    if(bytes_written != (ssize_t)write_len) {
      write_len -= bytes_written;
      sptr += bytes_written;
    }
    else
      break;
  }

  return result;
}

/***********************************************************************
 *
 * ftp_quit()
 *
 * This should be called before calling sclose() on an ftp control connection
 * (not data connections). We should then wait for the response from the
 * server before returning. The calling code should then try to close the
 * connection.
 *
 */
static CURLcode ftp_quit(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;

  if(conn->proto.ftpc.ctl_valid) {
    result = Curl_pp_sendf(&conn->proto.ftpc.pp, "%s", "QUIT");
    if(result) {
      failf(conn->data, "Failure sending QUIT command: %s",
            curl_easy_strerror(result));
      conn->proto.ftpc.ctl_valid = FALSE; /* mark control connection as bad */
      connclose(conn, "QUIT command failed"); /* mark for connection closure */
      state(conn, FTP_STOP);
      return result;
    }

    state(conn, FTP_QUIT);

    result = ftp_block_statemach(conn);
  }

  return result;
}

/***********************************************************************
 *
 * ftp_disconnect()
 *
 * Disconnect from an FTP server. Cleanup protocol-specific per-connection
 * resources. BLOCKING.
 */
static CURLcode ftp_disconnect(struct connectdata *conn, bool dead_connection)
{
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  struct pingpong *pp = &ftpc->pp;

  /* We cannot send quit unconditionally. If this connection is stale or
     bad in any way, sending quit and waiting around here will make the
     disconnect wait in vain and cause more problems than we need to.

     ftp_quit() will check the state of ftp->ctl_valid. If it's ok it
     will try to send the QUIT command, otherwise it will just return.
  */
  if(dead_connection)
    ftpc->ctl_valid = FALSE;

  /* The FTP session may or may not have been allocated/setup at this point! */
  (void)ftp_quit(conn); /* ignore errors on the QUIT */

  if(ftpc->entrypath) {
    struct Curl_easy *data = conn->data;
    if(data->state.most_recent_ftp_entrypath == ftpc->entrypath) {
      data->state.most_recent_ftp_entrypath = NULL;
    }
    free(ftpc->entrypath);
    ftpc->entrypath = NULL;
  }

  freedirs(ftpc);
  free(ftpc->prevpath);
  ftpc->prevpath = NULL;
  free(ftpc->server_os);
  ftpc->server_os = NULL;

  Curl_pp_disconnect(pp);

#ifdef HAVE_GSSAPI
  Curl_sec_end(conn);
#endif

  return CURLE_OK;
}

/***********************************************************************
 *
 * ftp_parse_url_path()
 *
 * Parse the URL path into separate path components.
 *
 */
static
CURLcode ftp_parse_url_path(struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  /* the ftp struct is already inited in ftp_connect() */
  struct FTP *ftp = data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  const char *slash_pos;  /* position of the first '/' char in curpos */
  const char *path_to_use = ftp->path;
  const char *cur_pos;
  const char *filename = NULL;

  cur_pos = path_to_use; /* current position in path. point at the begin of
                            next path component */

  ftpc->ctl_valid = FALSE;
  ftpc->cwdfail = FALSE;

  switch(data->set.ftp_filemethod) {
  case FTPFILE_NOCWD:
    /* fastest, but less standard-compliant */

    /*
      The best time to check whether the path is a file or directory is right
      here. so:

      the first condition in the if() right here, is there just in case
      someone decides to set path to NULL one day
   */
    if(path_to_use[0] &&
       (path_to_use[strlen(path_to_use) - 1] != '/') )
      filename = path_to_use;  /* this is a full file path */
    /*
      else {
        ftpc->file is not used anywhere other than for operations on a file.
        In other words, never for directory operations.
        So we can safely leave filename as NULL here and use it as a
        argument in dir/file decisions.
      }
    */
    break;

  case FTPFILE_SINGLECWD:
    /* get the last slash */
    if(!path_to_use[0]) {
      /* no dir, no file */
      ftpc->dirdepth = 0;
      break;
    }
    slash_pos = strrchr(cur_pos, '/');
    if(slash_pos || !*cur_pos) {
      size_t dirlen = slash_pos-cur_pos;
      CURLcode result;

      ftpc->dirs = calloc(1, sizeof(ftpc->dirs[0]));
      if(!ftpc->dirs)
        return CURLE_OUT_OF_MEMORY;

      if(!dirlen)
        dirlen++;

      result = Curl_urldecode(conn->data, slash_pos ? cur_pos : "/",
                              slash_pos ? dirlen : 1,
                              &ftpc->dirs[0], NULL,
                              TRUE);
      if(result) {
        freedirs(ftpc);
        return result;
      }
      ftpc->dirdepth = 1; /* we consider it to be a single dir */
      filename = slash_pos ? slash_pos + 1 : cur_pos; /* rest is file name */
    }
    else
      filename = cur_pos;  /* this is a file name only */
    break;

  default: /* allow pretty much anything */
  case FTPFILE_MULTICWD:
    ftpc->dirdepth = 0;
    ftpc->diralloc = 5; /* default dir depth to allocate */
    ftpc->dirs = calloc(ftpc->diralloc, sizeof(ftpc->dirs[0]));
    if(!ftpc->dirs)
      return CURLE_OUT_OF_MEMORY;

    /* we have a special case for listing the root dir only */
    if(!strcmp(path_to_use, "/")) {
      cur_pos++; /* make it point to the zero byte */
      ftpc->dirs[0] = strdup("/");
      ftpc->dirdepth++;
    }
    else {
      /* parse the URL path into separate path components */
      while((slash_pos = strchr(cur_pos, '/')) != NULL) {
        /* 1 or 0 pointer offset to indicate absolute directory */
        ssize_t absolute_dir = ((cur_pos - ftp->path > 0) &&
                                (ftpc->dirdepth == 0))?1:0;

        /* seek out the next path component */
        if(slash_pos-cur_pos) {
          /* we skip empty path components, like "x//y" since the FTP command
             CWD requires a parameter and a non-existent parameter a) doesn't
             work on many servers and b) has no effect on the others. */
          size_t len = slash_pos - cur_pos + absolute_dir;
          CURLcode result =
            Curl_urldecode(conn->data, cur_pos - absolute_dir, len,
                           &ftpc->dirs[ftpc->dirdepth], NULL,
                           TRUE);
          if(result) {
            freedirs(ftpc);
            return result;
          }
        }
        else {
          cur_pos = slash_pos + 1; /* jump to the rest of the string */
          if(!ftpc->dirdepth) {
            /* path starts with a slash, add that as a directory */
            ftpc->dirs[ftpc->dirdepth] = strdup("/");
            if(!ftpc->dirs[ftpc->dirdepth++]) { /* run out of memory ... */
              failf(data, "no memory");
              freedirs(ftpc);
              return CURLE_OUT_OF_MEMORY;
            }
          }
          continue;
        }

        cur_pos = slash_pos + 1; /* jump to the rest of the string */
        if(++ftpc->dirdepth >= ftpc->diralloc) {
          /* enlarge array */
          char **bigger;
          ftpc->diralloc *= 2; /* double the size each time */
          bigger = realloc(ftpc->dirs, ftpc->diralloc * sizeof(ftpc->dirs[0]));
          if(!bigger) {
            freedirs(ftpc);
            return CURLE_OUT_OF_MEMORY;
          }
          ftpc->dirs = bigger;
        }
      }
    }
    filename = cur_pos;  /* the rest is the file name */
    break;
  } /* switch */

  if(filename && *filename) {
    CURLcode result =
      Curl_urldecode(conn->data, filename, 0,  &ftpc->file, NULL, TRUE);

    if(result) {
      freedirs(ftpc);
      return result;
    }
  }
  else
    ftpc->file = NULL; /* instead of point to a zero byte, we make it a NULL
                          pointer */

  if(data->set.upload && !ftpc->file && (ftp->transfer == FTPTRANSFER_BODY)) {
    /* We need a file name when uploading. Return error! */
    failf(data, "Uploading to a URL without a file name!");
    return CURLE_URL_MALFORMAT;
  }

  ftpc->cwddone = FALSE; /* default to not done */

  if(ftpc->prevpath) {
    /* prevpath is "raw" so we convert the input path before we compare the
       strings */
    size_t dlen;
    char *path;
    CURLcode result =
      Curl_urldecode(conn->data, ftp->path, 0, &path, &dlen, TRUE);
    if(result) {
      freedirs(ftpc);
      return result;
    }

    dlen -= ftpc->file?strlen(ftpc->file):0;
    if((dlen == strlen(ftpc->prevpath)) &&
       !strncmp(path, ftpc->prevpath, dlen) &&
       (ftpc->prevmethod == data->set.ftp_filemethod)) {
      infof(data, "Request has same path as previous transfer\n");
      ftpc->cwddone = TRUE;
    }
    free(path);
  }

  return CURLE_OK;
}

/* call this when the DO phase has completed */
static CURLcode ftp_dophase_done(struct connectdata *conn,
                                 bool connected)
{
  struct FTP *ftp = conn->data->req.protop;
  struct ftp_conn *ftpc = &conn->proto.ftpc;

  if(connected) {
    int completed;
    CURLcode result = ftp_do_more(conn, &completed);

    if(result) {
      close_secondarysocket(conn);
      return result;
    }
  }

  if(ftp->transfer != FTPTRANSFER_BODY)
    /* no data to transfer */
    Curl_setup_transfer(conn->data, -1, -1, FALSE, -1);
  else if(!connected)
    /* since we didn't connect now, we want do_more to get called */
    conn->bits.do_more = TRUE;

  ftpc->ctl_valid = TRUE; /* seems good */

  return CURLE_OK;
}

/* called from multi.c while DOing */
static CURLcode ftp_doing(struct connectdata *conn,
                          bool *dophase_done)
{
  CURLcode result = ftp_multi_statemach(conn, dophase_done);

  if(result)
    DEBUGF(infof(conn->data, "DO phase failed\n"));
  else if(*dophase_done) {
    result = ftp_dophase_done(conn, FALSE /* not connected */);

    DEBUGF(infof(conn->data, "DO phase is complete2\n"));
  }
  return result;
}

/***********************************************************************
 *
 * ftp_regular_transfer()
 *
 * The input argument is already checked for validity.
 *
 * Performs all commands done before a regular transfer between a local and a
 * remote host.
 *
 * ftp->ctl_valid starts out as FALSE, and gets set to TRUE if we reach the
 * ftp_done() function without finding any major problem.
 */
static
CURLcode ftp_regular_transfer(struct connectdata *conn,
                              bool *dophase_done)
{
  CURLcode result = CURLE_OK;
  bool connected = FALSE;
  struct Curl_easy *data = conn->data;
  struct ftp_conn *ftpc = &conn->proto.ftpc;
  data->req.size = -1; /* make sure this is unknown at this point */

  Curl_pgrsSetUploadCounter(data, 0);
  Curl_pgrsSetDownloadCounter(data, 0);
  Curl_pgrsSetUploadSize(data, -1);
  Curl_pgrsSetDownloadSize(data, -1);

  ftpc->ctl_valid = TRUE; /* starts good */

  result = ftp_perform(conn,
                       &connected, /* have we connected after PASV/PORT */
                       dophase_done); /* all commands in the DO-phase done? */

  if(!result) {

    if(!*dophase_done)
      /* the DO phase has not completed yet */
      return CURLE_OK;

    result = ftp_dophase_done(conn, connected);

    if(result)
      return result;
  }
  else
    freedirs(ftpc);

  return result;
}

static CURLcode ftp_setup_connection(struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  char *type;
  struct FTP *ftp;

  conn->data->req.protop = ftp = calloc(sizeof(struct FTP), 1);
  if(NULL == ftp)
    return CURLE_OUT_OF_MEMORY;

  ftp->path = &data->state.up.path[1]; /* don't include the initial slash */

  /* FTP URLs support an extension like ";type=<typecode>" that
   * we'll try to get now! */
  type = strstr(ftp->path, ";type=");

  if(!type)
    type = strstr(conn->host.rawalloc, ";type=");

  if(type) {
    char command;
    *type = 0;                     /* it was in the middle of the hostname */
    command = Curl_raw_toupper(type[6]);
    conn->bits.type_set = TRUE;

    switch(command) {
    case 'A': /* ASCII mode */
      data->set.prefer_ascii = TRUE;
      break;

    case 'D': /* directory mode */
      data->set.ftp_list_only = TRUE;
      break;

    case 'I': /* binary mode */
    default:
      /* switch off ASCII */
      data->set.prefer_ascii = FALSE;
      break;
    }
  }

  /* get some initial data into the ftp struct */
  ftp->transfer = FTPTRANSFER_BODY;
  ftp->downloadsize = 0;

  /* No need to duplicate user+password, the connectdata struct won't change
     during a session, but we re-init them here since on subsequent inits
     since the conn struct may have changed or been replaced.
  */
  ftp->user = conn->user;
  ftp->passwd = conn->passwd;
  if(isBadFtpString(ftp->user))
    return CURLE_URL_MALFORMAT;
  if(isBadFtpString(ftp->passwd))
    return CURLE_URL_MALFORMAT;

  conn->proto.ftpc.known_filesize = -1; /* unknown size for now */

  return CURLE_OK;
}

#endif /* CURL_DISABLE_FTP */
