/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al.
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

#ifndef CURL_DISABLE_RTSP

#include "urldata.h"
#include <curl/curl.h>
#include "transfer.h"
#include "sendf.h"
#include "multiif.h"
#include "http.h"
#include "url.h"
#include "progress.h"
#include "rtsp.h"
#include "strcase.h"
#include "select.h"
#include "connect.h"
#include "strdup.h"
/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

/*
 * TODO (general)
 *  -incoming server requests
 *      -server CSeq counter
 *  -digest authentication
 *  -connect through proxy
 *  -pipelining?
 */


#define RTP_PKT_CHANNEL(p)   ((int)((unsigned char)((p)[1])))

#define RTP_PKT_LENGTH(p)  ((((int)((unsigned char)((p)[2]))) << 8) | \
                             ((int)((unsigned char)((p)[3]))))

/* protocol-specific functions set up to be called by the main engine */
static CURLcode rtsp_do(struct connectdata *conn, bool *done);
static CURLcode rtsp_done(struct connectdata *conn, CURLcode, bool premature);
static CURLcode rtsp_connect(struct connectdata *conn, bool *done);
static CURLcode rtsp_disconnect(struct connectdata *conn, bool dead);

static int rtsp_getsock_do(struct connectdata *conn,
                           curl_socket_t *socks,
                           int numsocks);

/*
 * Parse and write out any available RTP data.
 *
 * nread: amount of data left after k->str. will be modified if RTP
 *        data is parsed and k->str is moved up
 * readmore: whether or not the RTP parser needs more data right away
 */
static CURLcode rtsp_rtp_readwrite(struct Curl_easy *data,
                                   struct connectdata *conn,
                                   ssize_t *nread,
                                   bool *readmore);

static CURLcode rtsp_setup_connection(struct connectdata *conn);

bool rtsp_connisdead(struct connectdata *check);
static unsigned int rtsp_conncheck(struct connectdata *check,
                                   unsigned int checks_to_perform);

/* this returns the socket to wait for in the DO and DOING state for the multi
   interface and then we're always _sending_ a request and thus we wait for
   the single socket to become writable only */
static int rtsp_getsock_do(struct connectdata *conn,
                           curl_socket_t *socks,
                           int numsocks)
{
  /* write mode */
  (void)numsocks; /* unused, we trust it to be at least 1 */
  socks[0] = conn->sock[FIRSTSOCKET];
  return GETSOCK_WRITESOCK(0);
}

static
CURLcode rtp_client_write(struct connectdata *conn, char *ptr, size_t len);


/*
 * RTSP handler interface.
 */
const struct Curl_handler Curl_handler_rtsp = {
  "RTSP",                               /* scheme */
  rtsp_setup_connection,                /* setup_connection */
  rtsp_do,                              /* do_it */
  rtsp_done,                            /* done */
  ZERO_NULL,                            /* do_more */
  rtsp_connect,                         /* connect_it */
  ZERO_NULL,                            /* connecting */
  ZERO_NULL,                            /* doing */
  ZERO_NULL,                            /* proto_getsock */
  rtsp_getsock_do,                      /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  ZERO_NULL,                            /* perform_getsock */
  rtsp_disconnect,                      /* disconnect */
  rtsp_rtp_readwrite,                   /* readwrite */
  rtsp_conncheck,                       /* connection_check */
  PORT_RTSP,                            /* defport */
  CURLPROTO_RTSP,                       /* protocol */
  PROTOPT_NONE                          /* flags */
};


static CURLcode rtsp_setup_connection(struct connectdata *conn)
{
  struct RTSP *rtsp;

  conn->data->req.protop = rtsp = calloc(1, sizeof(struct RTSP));
  if(!rtsp)
    return CURLE_OUT_OF_MEMORY;

  return CURLE_OK;
}


/*
 * The server may send us RTP data at any point, and RTSPREQ_RECEIVE does not
 * want to block the application forever while receiving a stream. Therefore,
 * we cannot assume that an RTSP socket is dead just because it is readable.
 *
 * Instead, if it is readable, run Curl_connalive() to peek at the socket
 * and distinguish between closed and data.
 */
bool rtsp_connisdead(struct connectdata *check)
{
  int sval;
  bool ret_val = TRUE;

  sval = SOCKET_READABLE(check->sock[FIRSTSOCKET], 0);
  if(sval == 0) {
    /* timeout */
    ret_val = FALSE;
  }
  else if(sval & CURL_CSELECT_ERR) {
    /* socket is in an error state */
    ret_val = TRUE;
  }
  else if(sval & CURL_CSELECT_IN) {
    /* readable with no error. could still be closed */
    ret_val = !Curl_connalive(check);
  }

  return ret_val;
}

/*
 * Function to check on various aspects of a connection.
 */
static unsigned int rtsp_conncheck(struct connectdata *check,
                                   unsigned int checks_to_perform)
{
  unsigned int ret_val = CONNRESULT_NONE;

  if(checks_to_perform & CONNCHECK_ISDEAD) {
    if(rtsp_connisdead(check))
      ret_val |= CONNRESULT_DEAD;
  }

  return ret_val;
}


static CURLcode rtsp_connect(struct connectdata *conn, bool *done)
{
  CURLcode httpStatus;
  struct Curl_easy *data = conn->data;

  httpStatus = Curl_http_connect(conn, done);

  /* Initialize the CSeq if not already done */
  if(data->state.rtsp_next_client_CSeq == 0)
    data->state.rtsp_next_client_CSeq = 1;
  if(data->state.rtsp_next_server_CSeq == 0)
    data->state.rtsp_next_server_CSeq = 1;

  conn->proto.rtspc.rtp_channel = -1;

  return httpStatus;
}

static CURLcode rtsp_disconnect(struct connectdata *conn, bool dead)
{
  (void) dead;
  Curl_safefree(conn->proto.rtspc.rtp_buf);
  return CURLE_OK;
}


static CURLcode rtsp_done(struct connectdata *conn,
                          CURLcode status, bool premature)
{
  struct Curl_easy *data = conn->data;
  struct RTSP *rtsp = data->req.protop;
  CURLcode httpStatus;

  /* Bypass HTTP empty-reply checks on receive */
  if(data->set.rtspreq == RTSPREQ_RECEIVE)
    premature = TRUE;

  httpStatus = Curl_http_done(conn, status, premature);

  if(rtsp) {
    /* Check the sequence numbers */
    long CSeq_sent = rtsp->CSeq_sent;
    long CSeq_recv = rtsp->CSeq_recv;
    if((data->set.rtspreq != RTSPREQ_RECEIVE) && (CSeq_sent != CSeq_recv)) {
      failf(data,
            "The CSeq of this request %ld did not match the response %ld",
            CSeq_sent, CSeq_recv);
      return CURLE_RTSP_CSEQ_ERROR;
    }
    if(data->set.rtspreq == RTSPREQ_RECEIVE &&
            (conn->proto.rtspc.rtp_channel == -1)) {
      infof(data, "Got an RTP Receive with a CSeq of %ld\n", CSeq_recv);
      /* TODO CPC: Server -> Client logic here */
    }
  }

  return httpStatus;
}

static CURLcode rtsp_do(struct connectdata *conn, bool *done)
{
  struct Curl_easy *data = conn->data;
  CURLcode result = CURLE_OK;
  Curl_RtspReq rtspreq = data->set.rtspreq;
  struct RTSP *rtsp = data->req.protop;
  struct HTTP *http;
  Curl_send_buffer *req_buffer;
  curl_off_t postsize = 0; /* for ANNOUNCE and SET_PARAMETER */
  curl_off_t putsize = 0; /* for ANNOUNCE and SET_PARAMETER */

  const char *p_request = NULL;
  const char *p_session_id = NULL;
  const char *p_accept = NULL;
  const char *p_accept_encoding = NULL;
  const char *p_range = NULL;
  const char *p_referrer = NULL;
  const char *p_stream_uri = NULL;
  const char *p_transport = NULL;
  const char *p_uagent = NULL;
  const char *p_proxyuserpwd = NULL;
  const char *p_userpwd = NULL;

  *done = TRUE;

  http = &(rtsp->http_wrapper);
  /* Assert that no one has changed the RTSP struct in an evil way */
  DEBUGASSERT((void *)http == (void *)rtsp);

  rtsp->CSeq_sent = data->state.rtsp_next_client_CSeq;
  rtsp->CSeq_recv = 0;

  /* Setup the 'p_request' pointer to the proper p_request string
   * Since all RTSP requests are included here, there is no need to
   * support custom requests like HTTP.
   **/
  data->set.opt_no_body = TRUE; /* most requests don't contain a body */
  switch(rtspreq) {
  default:
    failf(data, "Got invalid RTSP request");
    return CURLE_BAD_FUNCTION_ARGUMENT;
  case RTSPREQ_OPTIONS:
    p_request = "OPTIONS";
    break;
  case RTSPREQ_DESCRIBE:
    p_request = "DESCRIBE";
    data->set.opt_no_body = FALSE;
    break;
  case RTSPREQ_ANNOUNCE:
    p_request = "ANNOUNCE";
    break;
  case RTSPREQ_SETUP:
    p_request = "SETUP";
    break;
  case RTSPREQ_PLAY:
    p_request = "PLAY";
    break;
  case RTSPREQ_PAUSE:
    p_request = "PAUSE";
    break;
  case RTSPREQ_TEARDOWN:
    p_request = "TEARDOWN";
    break;
  case RTSPREQ_GET_PARAMETER:
    /* GET_PARAMETER's no_body status is determined later */
    p_request = "GET_PARAMETER";
    data->set.opt_no_body = FALSE;
    break;
  case RTSPREQ_SET_PARAMETER:
    p_request = "SET_PARAMETER";
    break;
  case RTSPREQ_RECORD:
    p_request = "RECORD";
    break;
  case RTSPREQ_RECEIVE:
    p_request = "";
    /* Treat interleaved RTP as body*/
    data->set.opt_no_body = FALSE;
    break;
  case RTSPREQ_LAST:
    failf(data, "Got invalid RTSP request: RTSPREQ_LAST");
    return CURLE_BAD_FUNCTION_ARGUMENT;
  }

  if(rtspreq == RTSPREQ_RECEIVE) {
    Curl_setup_transfer(conn, FIRSTSOCKET, -1, TRUE,
                        &http->readbytecount, -1, NULL);

    return result;
  }

  p_session_id = data->set.str[STRING_RTSP_SESSION_ID];
  if(!p_session_id &&
     (rtspreq & ~(RTSPREQ_OPTIONS | RTSPREQ_DESCRIBE | RTSPREQ_SETUP))) {
    failf(data, "Refusing to issue an RTSP request [%s] without a session ID.",
          p_request);
    return CURLE_BAD_FUNCTION_ARGUMENT;
  }

  /* TODO: proxy? */

  /* Stream URI. Default to server '*' if not specified */
  if(data->set.str[STRING_RTSP_STREAM_URI]) {
    p_stream_uri = data->set.str[STRING_RTSP_STREAM_URI];
  }
  else {
    p_stream_uri = "*";
  }

  /* Transport Header for SETUP requests */
  p_transport = Curl_checkheaders(conn, "Transport");
  if(rtspreq == RTSPREQ_SETUP && !p_transport) {
    /* New Transport: setting? */
    if(data->set.str[STRING_RTSP_TRANSPORT]) {
      Curl_safefree(conn->allocptr.rtsp_transport);

      conn->allocptr.rtsp_transport =
        aprintf("Transport: %s\r\n",
                data->set.str[STRING_RTSP_TRANSPORT]);
      if(!conn->allocptr.rtsp_transport)
        return CURLE_OUT_OF_MEMORY;
    }
    else {
      failf(data,
            "Refusing to issue an RTSP SETUP without a Transport: header.");
      return CURLE_BAD_FUNCTION_ARGUMENT;
    }

    p_transport = conn->allocptr.rtsp_transport;
  }

  /* Accept Headers for DESCRIBE requests */
  if(rtspreq == RTSPREQ_DESCRIBE) {
    /* Accept Header */
    p_accept = Curl_checkheaders(conn, "Accept")?
      NULL:"Accept: application/sdp\r\n";

    /* Accept-Encoding header */
    if(!Curl_checkheaders(conn, "Accept-Encoding") &&
       data->set.str[STRING_ENCODING]) {
      Curl_safefree(conn->allocptr.accept_encoding);
      conn->allocptr.accept_encoding =
        aprintf("Accept-Encoding: %s\r\n", data->set.str[STRING_ENCODING]);

      if(!conn->allocptr.accept_encoding)
        return CURLE_OUT_OF_MEMORY;

      p_accept_encoding = conn->allocptr.accept_encoding;
    }
  }

  /* The User-Agent string might have been allocated in url.c already, because
     it might have been used in the proxy connect, but if we have got a header
     with the user-agent string specified, we erase the previously made string
     here. */
  if(Curl_checkheaders(conn, "User-Agent") && conn->allocptr.uagent) {
    Curl_safefree(conn->allocptr.uagent);
    conn->allocptr.uagent = NULL;
  }
  else if(!Curl_checkheaders(conn, "User-Agent") &&
          data->set.str[STRING_USERAGENT]) {
    p_uagent = conn->allocptr.uagent;
  }

  /* setup the authentication headers */
  result = Curl_http_output_auth(conn, p_request, p_stream_uri, FALSE);
  if(result)
    return result;

  p_proxyuserpwd = conn->allocptr.proxyuserpwd;
  p_userpwd = conn->allocptr.userpwd;

  /* Referrer */
  Curl_safefree(conn->allocptr.ref);
  if(data->change.referer && !Curl_checkheaders(conn, "Referer"))
    conn->allocptr.ref = aprintf("Referer: %s\r\n", data->change.referer);
  else
    conn->allocptr.ref = NULL;

  p_referrer = conn->allocptr.ref;

  /*
   * Range Header
   * Only applies to PLAY, PAUSE, RECORD
   *
   * Go ahead and use the Range stuff supplied for HTTP
   */
  if(data->state.use_range &&
     (rtspreq  & (RTSPREQ_PLAY | RTSPREQ_PAUSE | RTSPREQ_RECORD))) {

    /* Check to see if there is a range set in the custom headers */
    if(!Curl_checkheaders(conn, "Range") && data->state.range) {
      Curl_safefree(conn->allocptr.rangeline);
      conn->allocptr.rangeline = aprintf("Range: %s\r\n", data->state.range);
      p_range = conn->allocptr.rangeline;
    }
  }

  /*
   * Sanity check the custom headers
   */
  if(Curl_checkheaders(conn, "CSeq")) {
    failf(data, "CSeq cannot be set as a custom header.");
    return CURLE_RTSP_CSEQ_ERROR;
  }
  if(Curl_checkheaders(conn, "Session")) {
    failf(data, "Session ID cannot be set as a custom header.");
    return CURLE_BAD_FUNCTION_ARGUMENT;
  }

  /* Initialize a dynamic send buffer */
  req_buffer = Curl_add_buffer_init();

  if(!req_buffer)
    return CURLE_OUT_OF_MEMORY;

  result =
    Curl_add_bufferf(&req_buffer,
                     "%s %s RTSP/1.0\r\n" /* Request Stream-URI RTSP/1.0 */
                     "CSeq: %ld\r\n", /* CSeq */
                     p_request, p_stream_uri, rtsp->CSeq_sent);
  if(result)
    return result;

  /*
   * Rather than do a normal alloc line, keep the session_id unformatted
   * to make comparison easier
   */
  if(p_session_id) {
    result = Curl_add_bufferf(&req_buffer, "Session: %s\r\n", p_session_id);
    if(result)
      return result;
  }

  /*
   * Shared HTTP-like options
   */
  result = Curl_add_bufferf(&req_buffer,
                            "%s" /* transport */
                            "%s" /* accept */
                            "%s" /* accept-encoding */
                            "%s" /* range */
                            "%s" /* referrer */
                            "%s" /* user-agent */
                            "%s" /* proxyuserpwd */
                            "%s" /* userpwd */
                            ,
                            p_transport ? p_transport : "",
                            p_accept ? p_accept : "",
                            p_accept_encoding ? p_accept_encoding : "",
                            p_range ? p_range : "",
                            p_referrer ? p_referrer : "",
                            p_uagent ? p_uagent : "",
                            p_proxyuserpwd ? p_proxyuserpwd : "",
                            p_userpwd ? p_userpwd : "");

  /*
   * Free userpwd now --- cannot reuse this for Negotiate and possibly NTLM
   * with basic and digest, it will be freed anyway by the next request
   */
  Curl_safefree(conn->allocptr.userpwd);
  conn->allocptr.userpwd = NULL;

  if(result)
    return result;

  if((rtspreq == RTSPREQ_SETUP) || (rtspreq == RTSPREQ_DESCRIBE)) {
    result = Curl_add_timecondition(data, req_buffer);
    if(result)
      return result;
  }

  result = Curl_add_custom_headers(conn, FALSE, req_buffer);
  if(result)
    return result;

  if(rtspreq == RTSPREQ_ANNOUNCE ||
     rtspreq == RTSPREQ_SET_PARAMETER ||
     rtspreq == RTSPREQ_GET_PARAMETER) {

    if(data->set.upload) {
      putsize = data->state.infilesize;
      data->set.httpreq = HTTPREQ_PUT;

    }
    else {
      postsize = (data->state.infilesize != -1)?
        data->state.infilesize:
        (data->set.postfields? (curl_off_t)strlen(data->set.postfields):0);
      data->set.httpreq = HTTPREQ_POST;
    }

    if(putsize > 0 || postsize > 0) {
      /* As stated in the http comments, it is probably not wise to
       * actually set a custom Content-Length in the headers */
      if(!Curl_checkheaders(conn, "Content-Length")) {
        result =
          Curl_add_bufferf(&req_buffer,
                           "Content-Length: %" CURL_FORMAT_CURL_OFF_T"\r\n",
                           (data->set.upload ? putsize : postsize));
        if(result)
          return result;
      }

      if(rtspreq == RTSPREQ_SET_PARAMETER ||
         rtspreq == RTSPREQ_GET_PARAMETER) {
        if(!Curl_checkheaders(conn, "Content-Type")) {
          result = Curl_add_bufferf(&req_buffer,
                                    "Content-Type: text/parameters\r\n");
          if(result)
            return result;
        }
      }

      if(rtspreq == RTSPREQ_ANNOUNCE) {
        if(!Curl_checkheaders(conn, "Content-Type")) {
          result = Curl_add_bufferf(&req_buffer,
                                    "Content-Type: application/sdp\r\n");
          if(result)
            return result;
        }
      }

      data->state.expect100header = FALSE; /* RTSP posts are simple/small */
    }
    else if(rtspreq == RTSPREQ_GET_PARAMETER) {
      /* Check for an empty GET_PARAMETER (heartbeat) request */
      data->set.httpreq = HTTPREQ_HEAD;
      data->set.opt_no_body = TRUE;
    }
  }

  /* RTSP never allows chunked transfer */
  data->req.forbidchunk = TRUE;
  /* Finish the request buffer */
  result = Curl_add_buffer(&req_buffer, "\r\n", 2);
  if(result)
    return result;

  if(postsize > 0) {
    result = Curl_add_buffer(&req_buffer, data->set.postfields,
                             (size_t)postsize);
    if(result)
      return result;
  }

  /* issue the request */
  result = Curl_add_buffer_send(&req_buffer, conn,
                                &data->info.request_size, 0, FIRSTSOCKET);
  if(result) {
    failf(data, "Failed sending RTSP request");
    return result;
  }

  Curl_setup_transfer(conn, FIRSTSOCKET, -1, TRUE, &http->readbytecount,
                      putsize?FIRSTSOCKET:-1,
                      putsize?&http->writebytecount:NULL);

  /* Increment the CSeq on success */
  data->state.rtsp_next_client_CSeq++;

  if(http->writebytecount) {
    /* if a request-body has been sent off, we make sure this progress is
       noted properly */
    Curl_pgrsSetUploadCounter(data, http->writebytecount);
    if(Curl_pgrsUpdate(conn))
      result = CURLE_ABORTED_BY_CALLBACK;
  }

  return result;
}


static CURLcode rtsp_rtp_readwrite(struct Curl_easy *data,
                                   struct connectdata *conn,
                                   ssize_t *nread,
                                   bool *readmore) {
  struct SingleRequest *k = &data->req;
  struct rtsp_conn *rtspc = &(conn->proto.rtspc);

  char *rtp; /* moving pointer to rtp data */
  ssize_t rtp_dataleft; /* how much data left to parse in this round */
  char *scratch;
  CURLcode result;

  if(rtspc->rtp_buf) {
    /* There was some leftover data the last time. Merge buffers */
    char *newptr = Curl_saferealloc(rtspc->rtp_buf,
                                    rtspc->rtp_bufsize + *nread);
    if(!newptr) {
      rtspc->rtp_buf = NULL;
      rtspc->rtp_bufsize = 0;
      return CURLE_OUT_OF_MEMORY;
    }
    rtspc->rtp_buf = newptr;
    memcpy(rtspc->rtp_buf + rtspc->rtp_bufsize, k->str, *nread);
    rtspc->rtp_bufsize += *nread;
    rtp = rtspc->rtp_buf;
    rtp_dataleft = rtspc->rtp_bufsize;
  }
  else {
    /* Just parse the request buffer directly */
    rtp = k->str;
    rtp_dataleft = *nread;
  }

  while((rtp_dataleft > 0) &&
        (rtp[0] == '$')) {
    if(rtp_dataleft > 4) {
      int rtp_length;

      /* Parse the header */
      /* The channel identifier immediately follows and is 1 byte */
      rtspc->rtp_channel = RTP_PKT_CHANNEL(rtp);

      /* The length is two bytes */
      rtp_length = RTP_PKT_LENGTH(rtp);

      if(rtp_dataleft < rtp_length + 4) {
        /* Need more - incomplete payload*/
        *readmore = TRUE;
        break;
      }
      /* We have the full RTP interleaved packet
       * Write out the header including the leading '$' */
      DEBUGF(infof(data, "RTP write channel %d rtp_length %d\n",
             rtspc->rtp_channel, rtp_length));
      result = rtp_client_write(conn, &rtp[0], rtp_length + 4);
      if(result) {
        failf(data, "Got an error writing an RTP packet");
        *readmore = FALSE;
        Curl_safefree(rtspc->rtp_buf);
        rtspc->rtp_buf = NULL;
        rtspc->rtp_bufsize = 0;
        return result;
      }

      /* Move forward in the buffer */
      rtp_dataleft -= rtp_length + 4;
      rtp += rtp_length + 4;

      if(data->set.rtspreq == RTSPREQ_RECEIVE) {
        /* If we are in a passive receive, give control back
         * to the app as often as we can.
         */
        k->keepon &= ~KEEP_RECV;
      }
    }
    else {
      /* Need more - incomplete header */
      *readmore = TRUE;
      break;
    }
  }

  if(rtp_dataleft != 0 && rtp[0] == '$') {
    DEBUGF(infof(data, "RTP Rewinding %zd %s\n", rtp_dataleft,
          *readmore ? "(READMORE)" : ""));

    /* Store the incomplete RTP packet for a "rewind" */
    scratch = malloc(rtp_dataleft);
    if(!scratch) {
      Curl_safefree(rtspc->rtp_buf);
      rtspc->rtp_buf = NULL;
      rtspc->rtp_bufsize = 0;
      return CURLE_OUT_OF_MEMORY;
    }
    memcpy(scratch, rtp, rtp_dataleft);
    Curl_safefree(rtspc->rtp_buf);
    rtspc->rtp_buf = scratch;
    rtspc->rtp_bufsize = rtp_dataleft;

    /* As far as the transfer is concerned, this data is consumed */
    *nread = 0;
    return CURLE_OK;
  }
  /* Fix up k->str to point just after the last RTP packet */
  k->str += *nread - rtp_dataleft;

  /* either all of the data has been read or...
   * rtp now points at the next byte to parse
   */
  if(rtp_dataleft > 0)
    DEBUGASSERT(k->str[0] == rtp[0]);

  DEBUGASSERT(rtp_dataleft <= *nread); /* sanity check */

  *nread = rtp_dataleft;

  /* If we get here, we have finished with the leftover/merge buffer */
  Curl_safefree(rtspc->rtp_buf);
  rtspc->rtp_buf = NULL;
  rtspc->rtp_bufsize = 0;

  return CURLE_OK;
}

static
CURLcode rtp_client_write(struct connectdata *conn, char *ptr, size_t len)
{
  struct Curl_easy *data = conn->data;
  size_t wrote;
  curl_write_callback writeit;
  void *user_ptr;

  if(len == 0) {
    failf(data, "Cannot write a 0 size RTP packet.");
    return CURLE_WRITE_ERROR;
  }

  /* If the user has configured CURLOPT_INTERLEAVEFUNCTION then use that
     function and any configured CURLOPT_INTERLEAVEDATA to write out the RTP
     data. Otherwise, use the CURLOPT_WRITEFUNCTION with the CURLOPT_WRITEDATA
     pointer to write out the RTP data. */
  if(data->set.fwrite_rtp) {
    writeit = data->set.fwrite_rtp;
    user_ptr = data->set.rtp_out;
  }
  else {
    writeit = data->set.fwrite_func;
    user_ptr = data->set.out;
  }

  Curl_set_in_callback(data, true);
  wrote = writeit(ptr, 1, len, user_ptr);
  Curl_set_in_callback(data, false);

  if(CURL_WRITEFUNC_PAUSE == wrote) {
    failf(data, "Cannot pause RTP");
    return CURLE_WRITE_ERROR;
  }

  if(wrote != len) {
    failf(data, "Failed writing RTP data");
    return CURLE_WRITE_ERROR;
  }

  return CURLE_OK;
}

CURLcode Curl_rtsp_parseheader(struct connectdata *conn,
                               char *header)
{
  struct Curl_easy *data = conn->data;
  long CSeq = 0;

  if(checkprefix("CSeq:", header)) {
    /* Store the received CSeq. Match is verified in rtsp_done */
    int nc = sscanf(&header[4], ": %ld", &CSeq);
    if(nc == 1) {
      struct RTSP *rtsp = data->req.protop;
      rtsp->CSeq_recv = CSeq; /* mark the request */
      data->state.rtsp_CSeq_recv = CSeq; /* update the handle */
    }
    else {
      failf(data, "Unable to read the CSeq header: [%s]", header);
      return CURLE_RTSP_CSEQ_ERROR;
    }
  }
  else if(checkprefix("Session:", header)) {
    char *start;

    /* Find the first non-space letter */
    start = header + 8;
    while(*start && ISSPACE(*start))
      start++;

    if(!*start) {
      failf(data, "Got a blank Session ID");
    }
    else if(data->set.str[STRING_RTSP_SESSION_ID]) {
      /* If the Session ID is set, then compare */
      if(strncmp(start, data->set.str[STRING_RTSP_SESSION_ID],
                 strlen(data->set.str[STRING_RTSP_SESSION_ID]))  != 0) {
        failf(data, "Got RTSP Session ID Line [%s], but wanted ID [%s]",
              start, data->set.str[STRING_RTSP_SESSION_ID]);
        return CURLE_RTSP_SESSION_ERROR;
      }
    }
    else {
      /* If the Session ID is not set, and we find it in a response, then set
       * it.
       *
       * Allow any non whitespace content, up to the field separator or end of
       * line. RFC 2326 isn't 100% clear on the session ID and for example
       * gstreamer does url-encoded session ID's not covered by the standard.
       */
      char *end = start;
      while(*end && *end != ';' && !ISSPACE(*end))
        end++;

      /* Copy the id substring into a new buffer */
      data->set.str[STRING_RTSP_SESSION_ID] = malloc(end - start + 1);
      if(data->set.str[STRING_RTSP_SESSION_ID] == NULL)
        return CURLE_OUT_OF_MEMORY;
      memcpy(data->set.str[STRING_RTSP_SESSION_ID], start, end - start);
      (data->set.str[STRING_RTSP_SESSION_ID])[end - start] = '\0';
    }
  }
  return CURLE_OK;
}

#endif /* CURL_DISABLE_RTSP */
