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

#ifdef USE_NGHTTP2
#include <nghttp2/nghttp2.h>
#include "urldata.h"
#include "http2.h"
#include "http.h"
#include "sendf.h"
#include "select.h"
#include "curl_base64.h"
#include "strcase.h"
#include "multiif.h"
#include "url.h"
#include "connect.h"
#include "strtoofft.h"
#include "strdup.h"
/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#define H2_BUFSIZE 32768

#if (NGHTTP2_VERSION_NUM < 0x010000)
#error too old nghttp2 version, upgrade!
#endif

#if (NGHTTP2_VERSION_NUM > 0x010800)
#define NGHTTP2_HAS_HTTP2_STRERROR 1
#endif

#if (NGHTTP2_VERSION_NUM >= 0x010900)
/* nghttp2_session_callbacks_set_error_callback is present in nghttp2 1.9.0 or
   later */
#define NGHTTP2_HAS_ERROR_CALLBACK 1
#else
#define nghttp2_session_callbacks_set_error_callback(x,y)
#endif

#if (NGHTTP2_VERSION_NUM >= 0x010c00)
#define NGHTTP2_HAS_SET_LOCAL_WINDOW_SIZE 1
#endif

#define HTTP2_HUGE_WINDOW_SIZE (1 << 30)

#ifdef DEBUG_HTTP2
#define H2BUGF(x) x
#else
#define H2BUGF(x) do { } WHILE_FALSE
#endif


static ssize_t http2_recv(struct connectdata *conn, int sockindex,
                          char *mem, size_t len, CURLcode *err);
static bool http2_connisdead(struct connectdata *conn);
static int h2_session_send(struct Curl_easy *data,
                           nghttp2_session *h2);
static int h2_process_pending_input(struct connectdata *conn,
                                    struct http_conn *httpc,
                                    CURLcode *err);

/*
 * Curl_http2_init_state() is called when the easy handle is created and
 * allows for HTTP/2 specific init of state.
 */
void Curl_http2_init_state(struct UrlState *state)
{
  state->stream_weight = NGHTTP2_DEFAULT_WEIGHT;
}

/*
 * Curl_http2_init_userset() is called when the easy handle is created and
 * allows for HTTP/2 specific user-set fields.
 */
void Curl_http2_init_userset(struct UserDefined *set)
{
  set->stream_weight = NGHTTP2_DEFAULT_WEIGHT;
}

static int http2_perform_getsock(const struct connectdata *conn,
                                 curl_socket_t *sock, /* points to
                                                         numsocks
                                                         number of
                                                         sockets */
                                 int numsocks)
{
  const struct http_conn *c = &conn->proto.httpc;
  struct SingleRequest *k = &conn->data->req;
  int bitmap = GETSOCK_BLANK;
  (void)numsocks;

  /* TODO We should check underlying socket state if it is SSL socket
     because of renegotiation. */
  sock[0] = conn->sock[FIRSTSOCKET];

  /* in a HTTP/2 connection we can basically always get a frame so we should
     always be ready for one */
  bitmap |= GETSOCK_READSOCK(FIRSTSOCKET);

  /* we're still uploading or the HTTP/2 layer wants to send data */
  if(((k->keepon & (KEEP_SEND|KEEP_SEND_PAUSE)) == KEEP_SEND) ||
     nghttp2_session_want_write(c->h2))
    bitmap |= GETSOCK_WRITESOCK(FIRSTSOCKET);

  return bitmap;
}

static int http2_getsock(struct connectdata *conn,
                         curl_socket_t *sock, /* points to numsocks
                                                 number of sockets */
                         int numsocks)
{
  return http2_perform_getsock(conn, sock, numsocks);
}

/*
 * http2_stream_free() free HTTP2 stream related data
 */
static void http2_stream_free(struct HTTP *http)
{
  if(http) {
    Curl_add_buffer_free(&http->header_recvbuf);
    Curl_add_buffer_free(&http->trailer_recvbuf);
    for(; http->push_headers_used > 0; --http->push_headers_used) {
      free(http->push_headers[http->push_headers_used - 1]);
    }
    free(http->push_headers);
    http->push_headers = NULL;
  }
}

/*
 * Disconnects *a* connection used for HTTP/2. It might be an old one from the
 * connection cache and not the "main" one. Don't touch the easy handle!
 */

static CURLcode http2_disconnect(struct connectdata *conn,
                                 bool dead_connection)
{
  struct http_conn *c = &conn->proto.httpc;
  (void)dead_connection;

  H2BUGF(infof(conn->data, "HTTP/2 DISCONNECT starts now\n"));

  nghttp2_session_del(c->h2);
  Curl_safefree(c->inbuf);

  H2BUGF(infof(conn->data, "HTTP/2 DISCONNECT done\n"));

  return CURLE_OK;
}

/*
 * The server may send us data at any point (e.g. PING frames). Therefore,
 * we cannot assume that an HTTP/2 socket is dead just because it is readable.
 *
 * Instead, if it is readable, run Curl_connalive() to peek at the socket
 * and distinguish between closed and data.
 */
static bool http2_connisdead(struct connectdata *conn)
{
  int sval;
  bool dead = TRUE;

  if(conn->bits.close)
    return TRUE;

  sval = SOCKET_READABLE(conn->sock[FIRSTSOCKET], 0);
  if(sval == 0) {
    /* timeout */
    dead = FALSE;
  }
  else if(sval & CURL_CSELECT_ERR) {
    /* socket is in an error state */
    dead = TRUE;
  }
  else if(sval & CURL_CSELECT_IN) {
    /* readable with no error. could still be closed */
    dead = !Curl_connalive(conn);
    if(!dead) {
      /* This happens before we've sent off a request and the connection is
         not in use by any other transfer, there shouldn't be any data here,
         only "protocol frames" */
      CURLcode result;
      struct http_conn *httpc = &conn->proto.httpc;
      ssize_t nread = -1;
      if(httpc->recv_underlying)
        /* if called "too early", this pointer isn't setup yet! */
        nread = ((Curl_recv *)httpc->recv_underlying)(
          conn, FIRSTSOCKET, httpc->inbuf, H2_BUFSIZE, &result);
      if(nread != -1) {
        infof(conn->data,
              "%d bytes stray data read before trying h2 connection\n",
              (int)nread);
        httpc->nread_inbuf = 0;
        httpc->inbuflen = nread;
        (void)h2_process_pending_input(conn, httpc, &result);
      }
      else
        /* the read failed so let's say this is dead anyway */
        dead = TRUE;
    }
  }

  return dead;
}

static unsigned int http2_conncheck(struct connectdata *check,
                                    unsigned int checks_to_perform)
{
  unsigned int ret_val = CONNRESULT_NONE;
  struct http_conn *c = &check->proto.httpc;
  int rc;
  bool send_frames = false;

  if(checks_to_perform & CONNCHECK_ISDEAD) {
    if(http2_connisdead(check))
      ret_val |= CONNRESULT_DEAD;
  }

  if(checks_to_perform & CONNCHECK_KEEPALIVE) {
    struct curltime now = Curl_now();
    time_t elapsed = Curl_timediff(now, check->keepalive);

    if(elapsed > check->upkeep_interval_ms) {
      /* Perform an HTTP/2 PING */
      rc = nghttp2_submit_ping(c->h2, 0, ZERO_NULL);
      if(!rc) {
        /* Successfully added a PING frame to the session. Need to flag this
           so the frame is sent. */
        send_frames = true;
      }
      else {
       failf(check->data, "nghttp2_submit_ping() failed: %s(%d)",
             nghttp2_strerror(rc), rc);
      }

      check->keepalive = now;
    }
  }

  if(send_frames) {
    rc = nghttp2_session_send(c->h2);
    if(rc)
      failf(check->data, "nghttp2_session_send() failed: %s(%d)",
            nghttp2_strerror(rc), rc);
  }

  return ret_val;
}

/* called from Curl_http_setup_conn */
void Curl_http2_setup_req(struct Curl_easy *data)
{
  struct HTTP *http = data->req.protop;

  http->nread_header_recvbuf = 0;
  http->bodystarted = FALSE;
  http->status_code = -1;
  http->pausedata = NULL;
  http->pauselen = 0;
  http->closed = FALSE;
  http->close_handled = FALSE;
  http->mem = data->state.buffer;
  http->len = data->set.buffer_size;
  http->memlen = 0;
}

/* called from Curl_http_setup_conn */
void Curl_http2_setup_conn(struct connectdata *conn)
{
  conn->proto.httpc.settings.max_concurrent_streams =
    DEFAULT_MAX_CONCURRENT_STREAMS;
  conn->proto.httpc.error_code = NGHTTP2_NO_ERROR;
}

/*
 * HTTP2 handler interface. This isn't added to the general list of protocols
 * but will be used at run-time when the protocol is dynamically switched from
 * HTTP to HTTP2.
 */
static const struct Curl_handler Curl_handler_http2 = {
  "HTTP",                               /* scheme */
  ZERO_NULL,                            /* setup_connection */
  Curl_http,                            /* do_it */
  Curl_http_done,                       /* done */
  ZERO_NULL,                            /* do_more */
  ZERO_NULL,                            /* connect_it */
  ZERO_NULL,                            /* connecting */
  ZERO_NULL,                            /* doing */
  http2_getsock,                        /* proto_getsock */
  http2_getsock,                        /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  http2_perform_getsock,                /* perform_getsock */
  http2_disconnect,                     /* disconnect */
  ZERO_NULL,                            /* readwrite */
  http2_conncheck,                      /* connection_check */
  PORT_HTTP,                            /* defport */
  CURLPROTO_HTTP,                       /* protocol */
  PROTOPT_STREAM                        /* flags */
};

static const struct Curl_handler Curl_handler_http2_ssl = {
  "HTTPS",                              /* scheme */
  ZERO_NULL,                            /* setup_connection */
  Curl_http,                            /* do_it */
  Curl_http_done,                       /* done */
  ZERO_NULL,                            /* do_more */
  ZERO_NULL,                            /* connect_it */
  ZERO_NULL,                            /* connecting */
  ZERO_NULL,                            /* doing */
  http2_getsock,                        /* proto_getsock */
  http2_getsock,                        /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  http2_perform_getsock,                /* perform_getsock */
  http2_disconnect,                     /* disconnect */
  ZERO_NULL,                            /* readwrite */
  http2_conncheck,                      /* connection_check */
  PORT_HTTP,                            /* defport */
  CURLPROTO_HTTPS,                      /* protocol */
  PROTOPT_SSL | PROTOPT_STREAM          /* flags */
};

/*
 * Store nghttp2 version info in this buffer, Prefix with a space.  Return
 * total length written.
 */
int Curl_http2_ver(char *p, size_t len)
{
  nghttp2_info *h2 = nghttp2_version(0);
  return msnprintf(p, len, " nghttp2/%s", h2->version_str);
}

/* HTTP/2 error code to name based on the Error Code Registry.
https://tools.ietf.org/html/rfc7540#page-77
nghttp2_error_code enums are identical.
*/
const char *Curl_http2_strerror(uint32_t err)
{
#ifndef NGHTTP2_HAS_HTTP2_STRERROR
  const char *str[] = {
    "NO_ERROR",             /* 0x0 */
    "PROTOCOL_ERROR",       /* 0x1 */
    "INTERNAL_ERROR",       /* 0x2 */
    "FLOW_CONTROL_ERROR",   /* 0x3 */
    "SETTINGS_TIMEOUT",     /* 0x4 */
    "STREAM_CLOSED",        /* 0x5 */
    "FRAME_SIZE_ERROR",     /* 0x6 */
    "REFUSED_STREAM",       /* 0x7 */
    "CANCEL",               /* 0x8 */
    "COMPRESSION_ERROR",    /* 0x9 */
    "CONNECT_ERROR",        /* 0xA */
    "ENHANCE_YOUR_CALM",    /* 0xB */
    "INADEQUATE_SECURITY",  /* 0xC */
    "HTTP_1_1_REQUIRED"     /* 0xD */
  };
  return (err < sizeof(str) / sizeof(str[0])) ? str[err] : "unknown";
#else
  return nghttp2_http2_strerror(err);
#endif
}

/*
 * The implementation of nghttp2_send_callback type. Here we write |data| with
 * size |length| to the network and return the number of bytes actually
 * written. See the documentation of nghttp2_send_callback for the details.
 */
static ssize_t send_callback(nghttp2_session *h2,
                             const uint8_t *data, size_t length, int flags,
                             void *userp)
{
  struct connectdata *conn = (struct connectdata *)userp;
  struct http_conn *c = &conn->proto.httpc;
  ssize_t written;
  CURLcode result = CURLE_OK;

  (void)h2;
  (void)flags;

  if(!c->send_underlying)
    /* called before setup properly! */
    return NGHTTP2_ERR_CALLBACK_FAILURE;

  written = ((Curl_send*)c->send_underlying)(conn, FIRSTSOCKET,
                                             data, length, &result);

  if(result == CURLE_AGAIN) {
    return NGHTTP2_ERR_WOULDBLOCK;
  }

  if(written == -1) {
    failf(conn->data, "Failed sending HTTP2 data");
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }

  if(!written)
    return NGHTTP2_ERR_WOULDBLOCK;

  return written;
}


/* We pass a pointer to this struct in the push callback, but the contents of
   the struct are hidden from the user. */
struct curl_pushheaders {
  struct Curl_easy *data;
  const nghttp2_push_promise *frame;
};

/*
 * push header access function. Only to be used from within the push callback
 */
char *curl_pushheader_bynum(struct curl_pushheaders *h, size_t num)
{
  /* Verify that we got a good easy handle in the push header struct, mostly to
     detect rubbish input fast(er). */
  if(!h || !GOOD_EASY_HANDLE(h->data))
    return NULL;
  else {
    struct HTTP *stream = h->data->req.protop;
    if(num < stream->push_headers_used)
      return stream->push_headers[num];
  }
  return NULL;
}

/*
 * push header access function. Only to be used from within the push callback
 */
char *curl_pushheader_byname(struct curl_pushheaders *h, const char *header)
{
  /* Verify that we got a good easy handle in the push header struct,
     mostly to detect rubbish input fast(er). Also empty header name
     is just a rubbish too. We have to allow ":" at the beginning of
     the header, but header == ":" must be rejected. If we have ':' in
     the middle of header, it could be matched in middle of the value,
     this is because we do prefix match.*/
  if(!h || !GOOD_EASY_HANDLE(h->data) || !header || !header[0] ||
     !strcmp(header, ":") || strchr(header + 1, ':'))
    return NULL;
  else {
    struct HTTP *stream = h->data->req.protop;
    size_t len = strlen(header);
    size_t i;
    for(i = 0; i<stream->push_headers_used; i++) {
      if(!strncmp(header, stream->push_headers[i], len)) {
        /* sub-match, make sure that it is followed by a colon */
        if(stream->push_headers[i][len] != ':')
          continue;
        return &stream->push_headers[i][len + 1];
      }
    }
  }
  return NULL;
}

/*
 * This specific transfer on this connection has been "drained".
 */
static void drained_transfer(struct Curl_easy *data,
                             struct http_conn *httpc)
{
  DEBUGASSERT(httpc->drain_total >= data->state.drain);
  httpc->drain_total -= data->state.drain;
  data->state.drain = 0;
}

/*
 * Mark this transfer to get "drained".
 */
static void drain_this(struct Curl_easy *data,
                       struct http_conn *httpc)
{
  data->state.drain++;
  httpc->drain_total++;
  DEBUGASSERT(httpc->drain_total >= data->state.drain);
}

static struct Curl_easy *duphandle(struct Curl_easy *data)
{
  struct Curl_easy *second = curl_easy_duphandle(data);
  if(second) {
    /* setup the request struct */
    struct HTTP *http = calloc(1, sizeof(struct HTTP));
    if(!http) {
      (void)Curl_close(second);
      second = NULL;
    }
    else {
      second->req.protop = http;
      http->header_recvbuf = Curl_add_buffer_init();
      if(!http->header_recvbuf) {
        free(http);
        (void)Curl_close(second);
        second = NULL;
      }
      else {
        Curl_http2_setup_req(second);
        second->state.stream_weight = data->state.stream_weight;
      }
    }
  }
  return second;
}


static int push_promise(struct Curl_easy *data,
                        struct connectdata *conn,
                        const nghttp2_push_promise *frame)
{
  int rv;
  H2BUGF(infof(data, "PUSH_PROMISE received, stream %u!\n",
               frame->promised_stream_id));
  if(data->multi->push_cb) {
    struct HTTP *stream;
    struct HTTP *newstream;
    struct curl_pushheaders heads;
    CURLMcode rc;
    struct http_conn *httpc;
    size_t i;
    /* clone the parent */
    struct Curl_easy *newhandle = duphandle(data);
    if(!newhandle) {
      infof(data, "failed to duplicate handle\n");
      rv = 1; /* FAIL HARD */
      goto fail;
    }

    heads.data = data;
    heads.frame = frame;
    /* ask the application */
    H2BUGF(infof(data, "Got PUSH_PROMISE, ask application!\n"));

    stream = data->req.protop;
    if(!stream) {
      failf(data, "Internal NULL stream!\n");
      (void)Curl_close(newhandle);
      rv = 1;
      goto fail;
    }

    Curl_set_in_callback(data, true);
    rv = data->multi->push_cb(data, newhandle,
                              stream->push_headers_used, &heads,
                              data->multi->push_userp);
    Curl_set_in_callback(data, false);

    /* free the headers again */
    for(i = 0; i<stream->push_headers_used; i++)
      free(stream->push_headers[i]);
    free(stream->push_headers);
    stream->push_headers = NULL;
    stream->push_headers_used = 0;

    if(rv) {
      /* denied, kill off the new handle again */
      http2_stream_free(newhandle->req.protop);
      newhandle->req.protop = NULL;
      (void)Curl_close(newhandle);
      goto fail;
    }

    newstream = newhandle->req.protop;
    newstream->stream_id = frame->promised_stream_id;
    newhandle->req.maxdownload = -1;
    newhandle->req.size = -1;

    /* approved, add to the multi handle and immediately switch to PERFORM
       state with the given connection !*/
    rc = Curl_multi_add_perform(data->multi, newhandle, conn);
    if(rc) {
      infof(data, "failed to add handle to multi\n");
      http2_stream_free(newhandle->req.protop);
      newhandle->req.protop = NULL;
      Curl_close(newhandle);
      rv = 1;
      goto fail;
    }

    httpc = &conn->proto.httpc;
    rv = nghttp2_session_set_stream_user_data(httpc->h2,
                                              frame->promised_stream_id,
                                              newhandle);
    if(rv) {
      infof(data, "failed to set user_data for stream %d\n",
            frame->promised_stream_id);
      DEBUGASSERT(0);
      goto fail;
    }
  }
  else {
    H2BUGF(infof(data, "Got PUSH_PROMISE, ignore it!\n"));
    rv = 1;
  }
  fail:
  return rv;
}

static int on_frame_recv(nghttp2_session *session, const nghttp2_frame *frame,
                         void *userp)
{
  struct connectdata *conn = (struct connectdata *)userp;
  struct http_conn *httpc = &conn->proto.httpc;
  struct Curl_easy *data_s = NULL;
  struct HTTP *stream = NULL;
  int rv;
  size_t left, ncopy;
  int32_t stream_id = frame->hd.stream_id;
  CURLcode result;

  if(!stream_id) {
    /* stream ID zero is for connection-oriented stuff */
    if(frame->hd.type == NGHTTP2_SETTINGS) {
      uint32_t max_conn = httpc->settings.max_concurrent_streams;
      H2BUGF(infof(conn->data, "Got SETTINGS\n"));
      httpc->settings.max_concurrent_streams =
        nghttp2_session_get_remote_settings(
          session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
      httpc->settings.enable_push =
        nghttp2_session_get_remote_settings(
          session, NGHTTP2_SETTINGS_ENABLE_PUSH);
      H2BUGF(infof(conn->data, "MAX_CONCURRENT_STREAMS == %d\n",
                   httpc->settings.max_concurrent_streams));
      H2BUGF(infof(conn->data, "ENABLE_PUSH == %s\n",
                   httpc->settings.enable_push?"TRUE":"false"));
      if(max_conn != httpc->settings.max_concurrent_streams) {
        /* only signal change if the value actually changed */
        infof(conn->data,
              "Connection state changed (MAX_CONCURRENT_STREAMS == %u)!\n",
              httpc->settings.max_concurrent_streams);
        Curl_multi_connchanged(conn->data->multi);
      }
    }
    return 0;
  }
  data_s = nghttp2_session_get_stream_user_data(session, stream_id);
  if(!data_s) {
    H2BUGF(infof(conn->data,
                 "No Curl_easy associated with stream: %x\n",
                 stream_id));
    return 0;
  }

  stream = data_s->req.protop;
  if(!stream) {
    H2BUGF(infof(data_s, "No proto pointer for stream: %x\n",
                 stream_id));
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }

  H2BUGF(infof(data_s, "on_frame_recv() header %x stream %x\n",
               frame->hd.type, stream_id));

  switch(frame->hd.type) {
  case NGHTTP2_DATA:
    /* If body started on this stream, then receiving DATA is illegal. */
    if(!stream->bodystarted) {
      rv = nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                     stream_id, NGHTTP2_PROTOCOL_ERROR);

      if(nghttp2_is_fatal(rv)) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
      }
    }
    break;
  case NGHTTP2_HEADERS:
    if(stream->bodystarted) {
      /* Only valid HEADERS after body started is trailer HEADERS.  We
         buffer them in on_header callback. */
      break;
    }

    /* nghttp2 guarantees that :status is received, and we store it to
       stream->status_code. Fuzzing has proven this can still be reached
       without status code having been set. */
    if(stream->status_code == -1)
      return NGHTTP2_ERR_CALLBACK_FAILURE;

    /* Only final status code signals the end of header */
    if(stream->status_code / 100 != 1) {
      stream->bodystarted = TRUE;
      stream->status_code = -1;
    }

    result = Curl_add_buffer(&stream->header_recvbuf, "\r\n", 2);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;

    left = stream->header_recvbuf->size_used - stream->nread_header_recvbuf;
    ncopy = CURLMIN(stream->len, left);

    memcpy(&stream->mem[stream->memlen],
           stream->header_recvbuf->buffer + stream->nread_header_recvbuf,
           ncopy);
    stream->nread_header_recvbuf += ncopy;

    H2BUGF(infof(data_s, "Store %zu bytes headers from stream %u at %p\n",
                 ncopy, stream_id, stream->mem));

    stream->len -= ncopy;
    stream->memlen += ncopy;

    drain_this(data_s, httpc);
    {
      /* get the pointer from userp again since it was re-assigned above */
      struct connectdata *conn_s = (struct connectdata *)userp;

      /* if we receive data for another handle, wake that up */
      if(conn_s->data != data_s)
        Curl_expire(data_s, 0, EXPIRE_RUN_NOW);
    }
    break;
  case NGHTTP2_PUSH_PROMISE:
    rv = push_promise(data_s, conn, &frame->push_promise);
    if(rv) { /* deny! */
      rv = nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                     frame->push_promise.promised_stream_id,
                                     NGHTTP2_CANCEL);
      if(nghttp2_is_fatal(rv)) {
        return rv;
      }
    }
    break;
  default:
    H2BUGF(infof(data_s, "Got frame type %x for stream %u!\n",
                 frame->hd.type, stream_id));
    break;
  }
  return 0;
}

static int on_data_chunk_recv(nghttp2_session *session, uint8_t flags,
                              int32_t stream_id,
                              const uint8_t *data, size_t len, void *userp)
{
  struct HTTP *stream;
  struct Curl_easy *data_s;
  size_t nread;
  struct connectdata *conn = (struct connectdata *)userp;
  (void)session;
  (void)flags;
  (void)data;

  DEBUGASSERT(stream_id); /* should never be a zero stream ID here */

  /* get the stream from the hash based on Stream ID */
  data_s = nghttp2_session_get_stream_user_data(session, stream_id);
  if(!data_s)
    /* Receiving a Stream ID not in the hash should not happen, this is an
       internal error more than anything else! */
    return NGHTTP2_ERR_CALLBACK_FAILURE;

  stream = data_s->req.protop;
  if(!stream)
    return NGHTTP2_ERR_CALLBACK_FAILURE;

  nread = CURLMIN(stream->len, len);
  memcpy(&stream->mem[stream->memlen], data, nread);

  stream->len -= nread;
  stream->memlen += nread;

  drain_this(data_s, &conn->proto.httpc);

  /* if we receive data for another handle, wake that up */
  if(conn->data != data_s)
    Curl_expire(data_s, 0, EXPIRE_RUN_NOW);

  H2BUGF(infof(data_s, "%zu data received for stream %u "
               "(%zu left in buffer %p, total %zu)\n",
               nread, stream_id,
               stream->len, stream->mem,
               stream->memlen));

  if(nread < len) {
    stream->pausedata = data + nread;
    stream->pauselen = len - nread;
    H2BUGF(infof(data_s, "NGHTTP2_ERR_PAUSE - %zu bytes out of buffer"
                 ", stream %u\n",
                 len - nread, stream_id));
    data_s->easy_conn->proto.httpc.pause_stream_id = stream_id;

    return NGHTTP2_ERR_PAUSE;
  }

  /* pause execution of nghttp2 if we received data for another handle
     in order to process them first. */
  if(conn->data != data_s) {
    data_s->easy_conn->proto.httpc.pause_stream_id = stream_id;

    return NGHTTP2_ERR_PAUSE;
  }

  return 0;
}

static int on_stream_close(nghttp2_session *session, int32_t stream_id,
                           uint32_t error_code, void *userp)
{
  struct Curl_easy *data_s;
  struct HTTP *stream;
  struct connectdata *conn = (struct connectdata *)userp;
  int rv;
  (void)session;
  (void)stream_id;

  if(stream_id) {
    struct http_conn *httpc;
    /* get the stream from the hash based on Stream ID, stream ID zero is for
       connection-oriented stuff */
    data_s = nghttp2_session_get_stream_user_data(session, stream_id);
    if(!data_s) {
      /* We could get stream ID not in the hash.  For example, if we
         decided to reject stream (e.g., PUSH_PROMISE). */
      return 0;
    }
    H2BUGF(infof(data_s, "on_stream_close(), %s (err %d), stream %u\n",
                 Curl_http2_strerror(error_code), error_code, stream_id));
    stream = data_s->req.protop;
    if(!stream)
      return NGHTTP2_ERR_CALLBACK_FAILURE;

    stream->closed = TRUE;
    httpc = &conn->proto.httpc;
    drain_this(data_s, httpc);
    httpc->error_code = error_code;

    /* remove the entry from the hash as the stream is now gone */
    rv = nghttp2_session_set_stream_user_data(session, stream_id, 0);
    if(rv) {
      infof(data_s, "http/2: failed to clear user_data for stream %d!\n",
            stream_id);
      DEBUGASSERT(0);
    }
    H2BUGF(infof(data_s, "Removed stream %u hash!\n", stream_id));
    stream->stream_id = 0; /* cleared */
  }
  return 0;
}

static int on_begin_headers(nghttp2_session *session,
                            const nghttp2_frame *frame, void *userp)
{
  struct HTTP *stream;
  struct Curl_easy *data_s = NULL;
  (void)userp;

  data_s = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
  if(!data_s) {
    return 0;
  }

  H2BUGF(infof(data_s, "on_begin_headers() was called\n"));

  if(frame->hd.type != NGHTTP2_HEADERS) {
    return 0;
  }

  stream = data_s->req.protop;
  if(!stream || !stream->bodystarted) {
    return 0;
  }

  if(!stream->trailer_recvbuf) {
    stream->trailer_recvbuf = Curl_add_buffer_init();
    if(!stream->trailer_recvbuf) {
      return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    }
  }
  return 0;
}

/* Decode HTTP status code.  Returns -1 if no valid status code was
   decoded. */
static int decode_status_code(const uint8_t *value, size_t len)
{
  int i;
  int res;

  if(len != 3) {
    return -1;
  }

  res = 0;

  for(i = 0; i < 3; ++i) {
    char c = value[i];

    if(c < '0' || c > '9') {
      return -1;
    }

    res *= 10;
    res += c - '0';
  }

  return res;
}

/* frame->hd.type is either NGHTTP2_HEADERS or NGHTTP2_PUSH_PROMISE */
static int on_header(nghttp2_session *session, const nghttp2_frame *frame,
                     const uint8_t *name, size_t namelen,
                     const uint8_t *value, size_t valuelen,
                     uint8_t flags,
                     void *userp)
{
  struct HTTP *stream;
  struct Curl_easy *data_s;
  int32_t stream_id = frame->hd.stream_id;
  struct connectdata *conn = (struct connectdata *)userp;
  CURLcode result;
  (void)flags;

  DEBUGASSERT(stream_id); /* should never be a zero stream ID here */

  /* get the stream from the hash based on Stream ID */
  data_s = nghttp2_session_get_stream_user_data(session, stream_id);
  if(!data_s)
    /* Receiving a Stream ID not in the hash should not happen, this is an
       internal error more than anything else! */
    return NGHTTP2_ERR_CALLBACK_FAILURE;

  stream = data_s->req.protop;
  if(!stream) {
    failf(data_s, "Internal NULL stream! 5\n");
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }

  /* Store received PUSH_PROMISE headers to be used when the subsequent
     PUSH_PROMISE callback comes */
  if(frame->hd.type == NGHTTP2_PUSH_PROMISE) {
    char *h;

    if(!stream->push_headers) {
      stream->push_headers_alloc = 10;
      stream->push_headers = malloc(stream->push_headers_alloc *
                                    sizeof(char *));
      if(!stream->push_headers)
        return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
      stream->push_headers_used = 0;
    }
    else if(stream->push_headers_used ==
            stream->push_headers_alloc) {
      char **headp;
      stream->push_headers_alloc *= 2;
      headp = Curl_saferealloc(stream->push_headers,
                               stream->push_headers_alloc * sizeof(char *));
      if(!headp) {
        stream->push_headers = NULL;
        return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
      }
      stream->push_headers = headp;
    }
    h = aprintf("%s:%s", name, value);
    if(h)
      stream->push_headers[stream->push_headers_used++] = h;
    return 0;
  }

  if(stream->bodystarted) {
    /* This is trailer fields. */
    /* 4 is for ": " and "\r\n". */
    uint32_t n = (uint32_t)(namelen + valuelen + 4);

    H2BUGF(infof(data_s, "h2 trailer: %.*s: %.*s\n", namelen, name, valuelen,
                 value));

    result = Curl_add_buffer(&stream->trailer_recvbuf, &n, sizeof(n));
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    result = Curl_add_buffer(&stream->trailer_recvbuf, name, namelen);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    result = Curl_add_buffer(&stream->trailer_recvbuf, ": ", 2);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    result = Curl_add_buffer(&stream->trailer_recvbuf, value, valuelen);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    result = Curl_add_buffer(&stream->trailer_recvbuf, "\r\n\0", 3);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;

    return 0;
  }

  if(namelen == sizeof(":status") - 1 &&
     memcmp(":status", name, namelen) == 0) {
    /* nghttp2 guarantees :status is received first and only once, and
       value is 3 digits status code, and decode_status_code always
       succeeds. */
    stream->status_code = decode_status_code(value, valuelen);
    DEBUGASSERT(stream->status_code != -1);

    result = Curl_add_buffer(&stream->header_recvbuf, "HTTP/2 ", 7);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    result = Curl_add_buffer(&stream->header_recvbuf, value, valuelen);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    /* the space character after the status code is mandatory */
    result = Curl_add_buffer(&stream->header_recvbuf, " \r\n", 3);
    if(result)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    /* if we receive data for another handle, wake that up */
    if(conn->data != data_s)
      Curl_expire(data_s, 0, EXPIRE_RUN_NOW);

    H2BUGF(infof(data_s, "h2 status: HTTP/2 %03d (easy %p)\n",
                 stream->status_code, data_s));
    return 0;
  }

  /* nghttp2 guarantees that namelen > 0, and :status was already
     received, and this is not pseudo-header field . */
  /* convert to a HTTP1-style header */
  result = Curl_add_buffer(&stream->header_recvbuf, name, namelen);
  if(result)
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  result = Curl_add_buffer(&stream->header_recvbuf, ": ", 2);
  if(result)
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  result = Curl_add_buffer(&stream->header_recvbuf, value, valuelen);
  if(result)
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  result = Curl_add_buffer(&stream->header_recvbuf, "\r\n", 2);
  if(result)
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  /* if we receive data for another handle, wake that up */
  if(conn->data != data_s)
    Curl_expire(data_s, 0, EXPIRE_RUN_NOW);

  H2BUGF(infof(data_s, "h2 header: %.*s: %.*s\n", namelen, name, valuelen,
               value));

  return 0; /* 0 is successful */
}

static ssize_t data_source_read_callback(nghttp2_session *session,
                                         int32_t stream_id,
                                         uint8_t *buf, size_t length,
                                         uint32_t *data_flags,
                                         nghttp2_data_source *source,
                                         void *userp)
{
  struct Curl_easy *data_s;
  struct HTTP *stream = NULL;
  size_t nread;
  (void)source;
  (void)userp;

  if(stream_id) {
    /* get the stream from the hash based on Stream ID, stream ID zero is for
       connection-oriented stuff */
    data_s = nghttp2_session_get_stream_user_data(session, stream_id);
    if(!data_s)
      /* Receiving a Stream ID not in the hash should not happen, this is an
         internal error more than anything else! */
      return NGHTTP2_ERR_CALLBACK_FAILURE;

    stream = data_s->req.protop;
    if(!stream)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
  }
  else
    return NGHTTP2_ERR_INVALID_ARGUMENT;

  nread = CURLMIN(stream->upload_len, length);
  if(nread > 0) {
    memcpy(buf, stream->upload_mem, nread);
    stream->upload_mem += nread;
    stream->upload_len -= nread;
    if(data_s->state.infilesize != -1)
      stream->upload_left -= nread;
  }

  if(stream->upload_left == 0)
    *data_flags = NGHTTP2_DATA_FLAG_EOF;
  else if(nread == 0)
    return NGHTTP2_ERR_DEFERRED;

  H2BUGF(infof(data_s, "data_source_read_callback: "
               "returns %zu bytes stream %u\n",
               nread, stream_id));

  return nread;
}

#if defined(NGHTTP2_HAS_ERROR_CALLBACK) &&      \
  !defined(CURL_DISABLE_VERBOSE_STRINGS)
static int error_callback(nghttp2_session *session,
                          const char *msg,
                          size_t len,
                          void *userp)
{
  struct connectdata *conn = (struct connectdata *)userp;
  (void)session;
  infof(conn->data, "http2 error: %.*s\n", len, msg);
  return 0;
}
#endif

static void populate_settings(struct connectdata *conn,
                              struct http_conn *httpc)
{
  nghttp2_settings_entry *iv = httpc->local_settings;

  iv[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
  iv[0].value = 100;

  iv[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
  iv[1].value = HTTP2_HUGE_WINDOW_SIZE;

  iv[2].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
  iv[2].value = conn->data->multi->push_cb != NULL;

  httpc->local_settings_num = 3;
}

void Curl_http2_done(struct connectdata *conn, bool premature)
{
  struct Curl_easy *data = conn->data;
  struct HTTP *http = data->req.protop;
  struct http_conn *httpc = &conn->proto.httpc;

  /* there might be allocated resources done before this got the 'h2' pointer
     setup */
  if(http->header_recvbuf) {
    Curl_add_buffer_free(&http->header_recvbuf);
    Curl_add_buffer_free(&http->trailer_recvbuf);
    if(http->push_headers) {
      /* if they weren't used and then freed before */
      for(; http->push_headers_used > 0; --http->push_headers_used) {
        free(http->push_headers[http->push_headers_used - 1]);
      }
      free(http->push_headers);
      http->push_headers = NULL;
    }
  }

  if(!httpc->h2) /* not HTTP/2 ? */
    return;

  if(data->state.drain)
    drained_transfer(data, httpc);

  if(premature) {
    /* RST_STREAM */
    if(!nghttp2_submit_rst_stream(httpc->h2, NGHTTP2_FLAG_NONE,
                                  http->stream_id, NGHTTP2_STREAM_CLOSED))
      (void)nghttp2_session_send(httpc->h2);

    if(http->stream_id == httpc->pause_stream_id) {
      infof(data, "stopped the pause stream!\n");
      httpc->pause_stream_id = 0;
    }
  }
  /* -1 means unassigned and 0 means cleared */
  if(http->stream_id > 0) {
    int rv = nghttp2_session_set_stream_user_data(httpc->h2,
                                                  http->stream_id, 0);
    if(rv) {
      infof(data, "http/2: failed to clear user_data for stream %d!\n",
            http->stream_id);
      DEBUGASSERT(0);
    }
    http->stream_id = 0;
  }
}

/*
 * Initialize nghttp2 for a Curl connection
 */
CURLcode Curl_http2_init(struct connectdata *conn)
{
  if(!conn->proto.httpc.h2) {
    int rc;
    nghttp2_session_callbacks *callbacks;

    conn->proto.httpc.inbuf = malloc(H2_BUFSIZE);
    if(conn->proto.httpc.inbuf == NULL)
      return CURLE_OUT_OF_MEMORY;

    rc = nghttp2_session_callbacks_new(&callbacks);

    if(rc) {
      failf(conn->data, "Couldn't initialize nghttp2 callbacks!");
      return CURLE_OUT_OF_MEMORY; /* most likely at least */
    }

    /* nghttp2_send_callback */
    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
    /* nghttp2_on_frame_recv_callback */
    nghttp2_session_callbacks_set_on_frame_recv_callback
      (callbacks, on_frame_recv);
    /* nghttp2_on_data_chunk_recv_callback */
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback
      (callbacks, on_data_chunk_recv);
    /* nghttp2_on_stream_close_callback */
    nghttp2_session_callbacks_set_on_stream_close_callback
      (callbacks, on_stream_close);
    /* nghttp2_on_begin_headers_callback */
    nghttp2_session_callbacks_set_on_begin_headers_callback
      (callbacks, on_begin_headers);
    /* nghttp2_on_header_callback */
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header);

#ifndef CURL_DISABLE_VERBOSE_STRINGS
    nghttp2_session_callbacks_set_error_callback(callbacks, error_callback);
#endif

    /* The nghttp2 session is not yet setup, do it */
    rc = nghttp2_session_client_new(&conn->proto.httpc.h2, callbacks, conn);

    nghttp2_session_callbacks_del(callbacks);

    if(rc) {
      failf(conn->data, "Couldn't initialize nghttp2!");
      return CURLE_OUT_OF_MEMORY; /* most likely at least */
    }
  }
  return CURLE_OK;
}

/*
 * Append headers to ask for a HTTP1.1 to HTTP2 upgrade.
 */
CURLcode Curl_http2_request_upgrade(Curl_send_buffer *req,
                                    struct connectdata *conn)
{
  CURLcode result;
  ssize_t binlen;
  char *base64;
  size_t blen;
  struct SingleRequest *k = &conn->data->req;
  uint8_t *binsettings = conn->proto.httpc.binsettings;
  struct http_conn *httpc = &conn->proto.httpc;

  populate_settings(conn, httpc);

  /* this returns number of bytes it wrote */
  binlen = nghttp2_pack_settings_payload(binsettings, H2_BINSETTINGS_LEN,
                                         httpc->local_settings,
                                         httpc->local_settings_num);
  if(!binlen) {
    failf(conn->data, "nghttp2 unexpectedly failed on pack_settings_payload");
    Curl_add_buffer_free(&req);
    return CURLE_FAILED_INIT;
  }
  conn->proto.httpc.binlen = binlen;

  result = Curl_base64url_encode(conn->data, (const char *)binsettings, binlen,
                                 &base64, &blen);
  if(result) {
    Curl_add_buffer_free(&req);
    return result;
  }

  result = Curl_add_bufferf(&req,
                            "Connection: Upgrade, HTTP2-Settings\r\n"
                            "Upgrade: %s\r\n"
                            "HTTP2-Settings: %s\r\n",
                            NGHTTP2_CLEARTEXT_PROTO_VERSION_ID, base64);
  free(base64);

  k->upgr101 = UPGR101_REQUESTED;

  return result;
}

/*
 * Returns nonzero if current HTTP/2 session should be closed.
 */
static int should_close_session(struct http_conn *httpc)
{
  return httpc->drain_total == 0 && !nghttp2_session_want_read(httpc->h2) &&
    !nghttp2_session_want_write(httpc->h2);
}

/*
 * h2_process_pending_input() processes pending input left in
 * httpc->inbuf.  Then, call h2_session_send() to send pending data.
 * This function returns 0 if it succeeds, or -1 and error code will
 * be assigned to *err.
 */
static int h2_process_pending_input(struct connectdata *conn,
                                    struct http_conn *httpc,
                                    CURLcode *err)
{
  ssize_t nread;
  char *inbuf;
  ssize_t rv;
  struct Curl_easy *data = conn->data;

  nread = httpc->inbuflen - httpc->nread_inbuf;
  inbuf = httpc->inbuf + httpc->nread_inbuf;

  rv = nghttp2_session_mem_recv(httpc->h2, (const uint8_t *)inbuf, nread);
  if(rv < 0) {
    failf(data,
          "h2_process_pending_input: nghttp2_session_mem_recv() returned "
          "%zd:%s\n", rv, nghttp2_strerror((int)rv));
    *err = CURLE_RECV_ERROR;
    return -1;
  }

  if(nread == rv) {
    H2BUGF(infof(data,
                 "h2_process_pending_input: All data in connection buffer "
                 "processed\n"));
    httpc->inbuflen = 0;
    httpc->nread_inbuf = 0;
  }
  else {
    httpc->nread_inbuf += rv;
    H2BUGF(infof(data,
                 "h2_process_pending_input: %zu bytes left in connection "
                 "buffer\n",
                 httpc->inbuflen - httpc->nread_inbuf));
  }

  rv = h2_session_send(data, httpc->h2);
  if(rv != 0) {
    *err = CURLE_SEND_ERROR;
    return -1;
  }

  if(should_close_session(httpc)) {
    H2BUGF(infof(data,
                 "h2_process_pending_input: nothing to do in this session\n"));
    if(httpc->error_code)
      *err = CURLE_HTTP2;
    else {
      /* not an error per se, but should still close the connection */
      connclose(conn, "GOAWAY received");
      *err = CURLE_OK;
    }
    return -1;
  }

  return 0;
}

/*
 * Called from transfer.c:done_sending when we stop uploading.
 */
CURLcode Curl_http2_done_sending(struct connectdata *conn)
{
  CURLcode result = CURLE_OK;

  if((conn->handler == &Curl_handler_http2_ssl) ||
     (conn->handler == &Curl_handler_http2)) {
    /* make sure this is only attempted for HTTP/2 transfers */

    struct HTTP *stream = conn->data->req.protop;

    if(stream->upload_left) {
      /* If the stream still thinks there's data left to upload. */
      struct http_conn *httpc = &conn->proto.httpc;
      nghttp2_session *h2 = httpc->h2;

      stream->upload_left = 0; /* DONE! */

      /* resume sending here to trigger the callback to get called again so
         that it can signal EOF to nghttp2 */
      (void)nghttp2_session_resume_data(h2, stream->stream_id);

      (void)h2_process_pending_input(conn, httpc, &result);
    }
  }
  return result;
}

static ssize_t http2_handle_stream_close(struct connectdata *conn,
                                         struct Curl_easy *data,
                                         struct HTTP *stream, CURLcode *err)
{
  char *trailer_pos, *trailer_end;
  CURLcode result;
  struct http_conn *httpc = &conn->proto.httpc;

  if(httpc->pause_stream_id == stream->stream_id) {
    httpc->pause_stream_id = 0;
  }

  drained_transfer(data, httpc);

  if(httpc->pause_stream_id == 0) {
    if(h2_process_pending_input(conn, httpc, err) != 0) {
      return -1;
    }
  }

  DEBUGASSERT(data->state.drain == 0);

  /* Reset to FALSE to prevent infinite loop in readwrite_data function. */
  stream->closed = FALSE;
  if(httpc->error_code == NGHTTP2_REFUSED_STREAM) {
    H2BUGF(infof(data, "REFUSED_STREAM (%d), try again on a new connection!\n",
                 stream->stream_id));
    connclose(conn, "REFUSED_STREAM"); /* don't use this anymore */
    data->state.refused_stream = TRUE;
    *err = CURLE_RECV_ERROR; /* trigger Curl_retry_request() later */
    return -1;
  }
  else if(httpc->error_code != NGHTTP2_NO_ERROR) {
    failf(data, "HTTP/2 stream %d was not closed cleanly: %s (err %u)",
          stream->stream_id, Curl_http2_strerror(httpc->error_code),
          httpc->error_code);
    *err = CURLE_HTTP2_STREAM;
    return -1;
  }

  if(!stream->bodystarted) {
    failf(data, "HTTP/2 stream %d was closed cleanly, but before getting "
          " all response header fields, treated as error",
          stream->stream_id);
    *err = CURLE_HTTP2_STREAM;
    return -1;
  }

  if(stream->trailer_recvbuf && stream->trailer_recvbuf->buffer) {
    trailer_pos = stream->trailer_recvbuf->buffer;
    trailer_end = trailer_pos + stream->trailer_recvbuf->size_used;

    for(; trailer_pos < trailer_end;) {
      uint32_t n;
      memcpy(&n, trailer_pos, sizeof(n));
      trailer_pos += sizeof(n);

      result = Curl_client_write(conn, CLIENTWRITE_HEADER, trailer_pos, n);
      if(result) {
        *err = result;
        return -1;
      }

      trailer_pos += n + 1;
    }
  }

  stream->close_handled = TRUE;

  H2BUGF(infof(data, "http2_recv returns 0, http2_handle_stream_close\n"));
  return 0;
}

/*
 * h2_pri_spec() fills in the pri_spec struct, used by nghttp2 to send weight
 * and dependency to the peer. It also stores the updated values in the state
 * struct.
 */

static void h2_pri_spec(struct Curl_easy *data,
                        nghttp2_priority_spec *pri_spec)
{
  struct HTTP *depstream = (data->set.stream_depends_on?
                            data->set.stream_depends_on->req.protop:NULL);
  int32_t depstream_id = depstream? depstream->stream_id:0;
  nghttp2_priority_spec_init(pri_spec, depstream_id, data->set.stream_weight,
                             data->set.stream_depends_e);
  data->state.stream_weight = data->set.stream_weight;
  data->state.stream_depends_e = data->set.stream_depends_e;
  data->state.stream_depends_on = data->set.stream_depends_on;
}

/*
 * h2_session_send() checks if there's been an update in the priority /
 * dependency settings and if so it submits a PRIORITY frame with the updated
 * info.
 */
static int h2_session_send(struct Curl_easy *data,
                           nghttp2_session *h2)
{
  struct HTTP *stream = data->req.protop;
  if((data->set.stream_weight != data->state.stream_weight) ||
     (data->set.stream_depends_e != data->state.stream_depends_e) ||
     (data->set.stream_depends_on != data->state.stream_depends_on) ) {
    /* send new weight and/or dependency */
    nghttp2_priority_spec pri_spec;
    int rv;

    h2_pri_spec(data, &pri_spec);

    H2BUGF(infof(data, "Queuing PRIORITY on stream %u (easy %p)\n",
                 stream->stream_id, data));
    rv = nghttp2_submit_priority(h2, NGHTTP2_FLAG_NONE, stream->stream_id,
                                 &pri_spec);
    if(rv)
      return rv;
  }

  return nghttp2_session_send(h2);
}

static ssize_t http2_recv(struct connectdata *conn, int sockindex,
                          char *mem, size_t len, CURLcode *err)
{
  CURLcode result = CURLE_OK;
  ssize_t rv;
  ssize_t nread;
  struct http_conn *httpc = &conn->proto.httpc;
  struct Curl_easy *data = conn->data;
  struct HTTP *stream = data->req.protop;

  (void)sockindex; /* we always do HTTP2 on sockindex 0 */

  if(should_close_session(httpc)) {
    H2BUGF(infof(data,
                 "http2_recv: nothing to do in this session\n"));
    *err = CURLE_HTTP2;
    return -1;
  }

  /* Nullify here because we call nghttp2_session_send() and they
     might refer to the old buffer. */
  stream->upload_mem = NULL;
  stream->upload_len = 0;

  /*
   * At this point 'stream' is just in the Curl_easy the connection
   * identifies as its owner at this time.
   */

  if(stream->bodystarted &&
     stream->nread_header_recvbuf < stream->header_recvbuf->size_used) {
    /* If there is body data pending for this stream to return, do that */
    size_t left =
      stream->header_recvbuf->size_used - stream->nread_header_recvbuf;
    size_t ncopy = CURLMIN(len, left);
    memcpy(mem, stream->header_recvbuf->buffer + stream->nread_header_recvbuf,
           ncopy);
    stream->nread_header_recvbuf += ncopy;

    H2BUGF(infof(data, "http2_recv: Got %d bytes from header_recvbuf\n",
                 (int)ncopy));
    return ncopy;
  }

  H2BUGF(infof(data, "http2_recv: easy %p (stream %u)\n",
               data, stream->stream_id));

  if((data->state.drain) && stream->memlen) {
    H2BUGF(infof(data, "http2_recv: DRAIN %zu bytes stream %u!! (%p => %p)\n",
                 stream->memlen, stream->stream_id,
                 stream->mem, mem));
    if(mem != stream->mem) {
      /* if we didn't get the same buffer this time, we must move the data to
         the beginning */
      memmove(mem, stream->mem, stream->memlen);
      stream->len = len - stream->memlen;
      stream->mem = mem;
    }
    if(httpc->pause_stream_id == stream->stream_id && !stream->pausedata) {
      /* We have paused nghttp2, but we have no pause data (see
         on_data_chunk_recv). */
      httpc->pause_stream_id = 0;
      if(h2_process_pending_input(conn, httpc, &result) != 0) {
        *err = result;
        return -1;
      }
    }
  }
  else if(stream->pausedata) {
    DEBUGASSERT(httpc->pause_stream_id == stream->stream_id);
    nread = CURLMIN(len, stream->pauselen);
    memcpy(mem, stream->pausedata, nread);

    stream->pausedata += nread;
    stream->pauselen -= nread;

    infof(data, "%zd data bytes written\n", nread);
    if(stream->pauselen == 0) {
      H2BUGF(infof(data, "Unpaused by stream %u\n", stream->stream_id));
      DEBUGASSERT(httpc->pause_stream_id == stream->stream_id);
      httpc->pause_stream_id = 0;

      stream->pausedata = NULL;
      stream->pauselen = 0;

      /* When NGHTTP2_ERR_PAUSE is returned from
         data_source_read_callback, we might not process DATA frame
         fully.  Calling nghttp2_session_mem_recv() again will
         continue to process DATA frame, but if there is no incoming
         frames, then we have to call it again with 0-length data.
         Without this, on_stream_close callback will not be called,
         and stream could be hanged. */
      if(h2_process_pending_input(conn, httpc, &result) != 0) {
        *err = result;
        return -1;
      }
    }
    H2BUGF(infof(data, "http2_recv: returns unpaused %zd bytes on stream %u\n",
                 nread, stream->stream_id));
    return nread;
  }
  else if(httpc->pause_stream_id) {
    /* If a stream paused nghttp2_session_mem_recv previously, and has
       not processed all data, it still refers to the buffer in
       nghttp2_session.  If we call nghttp2_session_mem_recv(), we may
       overwrite that buffer.  To avoid that situation, just return
       here with CURLE_AGAIN.  This could be busy loop since data in
       socket is not read.  But it seems that usually streams are
       notified with its drain property, and socket is read again
       quickly. */
    H2BUGF(infof(data, "stream %x is paused, pause id: %x\n",
                 stream->stream_id, httpc->pause_stream_id));
    *err = CURLE_AGAIN;
    return -1;
  }
  else {
    char *inbuf;
    /* remember where to store incoming data for this stream and how big the
       buffer is */
    stream->mem = mem;
    stream->len = len;
    stream->memlen = 0;

    if(httpc->inbuflen == 0) {
      nread = ((Curl_recv *)httpc->recv_underlying)(
          conn, FIRSTSOCKET, httpc->inbuf, H2_BUFSIZE, &result);

      if(nread == -1) {
        if(result != CURLE_AGAIN)
          failf(data, "Failed receiving HTTP2 data");
        else if(stream->closed)
          /* received when the stream was already closed! */
          return http2_handle_stream_close(conn, data, stream, err);

        *err = result;
        return -1;
      }

      if(nread == 0) {
        H2BUGF(infof(data, "end of stream\n"));
        *err = CURLE_OK;
        return 0;
      }

      H2BUGF(infof(data, "nread=%zd\n", nread));

      httpc->inbuflen = nread;
      inbuf = httpc->inbuf;
    }
    else {
      nread = httpc->inbuflen - httpc->nread_inbuf;
      inbuf = httpc->inbuf + httpc->nread_inbuf;

      H2BUGF(infof(data, "Use data left in connection buffer, nread=%zd\n",
                   nread));
    }
    rv = nghttp2_session_mem_recv(httpc->h2, (const uint8_t *)inbuf, nread);

    if(nghttp2_is_fatal((int)rv)) {
      failf(data, "nghttp2_session_mem_recv() returned %zd:%s\n",
            rv, nghttp2_strerror((int)rv));
      *err = CURLE_RECV_ERROR;
      return -1;
    }
    H2BUGF(infof(data, "nghttp2_session_mem_recv() returns %zd\n", rv));
    if(nread == rv) {
      H2BUGF(infof(data, "All data in connection buffer processed\n"));
      httpc->inbuflen = 0;
      httpc->nread_inbuf = 0;
    }
    else {
      httpc->nread_inbuf += rv;
      H2BUGF(infof(data, "%zu bytes left in connection buffer\n",
                   httpc->inbuflen - httpc->nread_inbuf));
    }
    /* Always send pending frames in nghttp2 session, because
       nghttp2_session_mem_recv() may queue new frame */
    rv = h2_session_send(data, httpc->h2);
    if(rv != 0) {
      *err = CURLE_SEND_ERROR;
      return -1;
    }

    if(should_close_session(httpc)) {
      H2BUGF(infof(data, "http2_recv: nothing to do in this session\n"));
      *err = CURLE_HTTP2;
      return -1;
    }
  }
  if(stream->memlen) {
    ssize_t retlen = stream->memlen;
    H2BUGF(infof(data, "http2_recv: returns %zd for stream %u\n",
                 retlen, stream->stream_id));
    stream->memlen = 0;

    if(httpc->pause_stream_id == stream->stream_id) {
      /* data for this stream is returned now, but this stream caused a pause
         already so we need it called again asap */
      H2BUGF(infof(data, "Data returned for PAUSED stream %u\n",
                   stream->stream_id));
    }
    else if(!stream->closed) {
      drained_transfer(data, httpc);
    }

    return retlen;
  }
  /* If stream is closed, return 0 to signal the http routine to close
     the connection */
  if(stream->closed) {
    return http2_handle_stream_close(conn, data, stream, err);
  }
  *err = CURLE_AGAIN;
  H2BUGF(infof(data, "http2_recv returns AGAIN for stream %u\n",
               stream->stream_id));
  return -1;
}

/* Index where :authority header field will appear in request header
   field list. */
#define AUTHORITY_DST_IDX 3

#define HEADER_OVERFLOW(x) \
  (x.namelen > (uint16_t)-1 || x.valuelen > (uint16_t)-1 - x.namelen)

/*
 * Check header memory for the token "trailers".
 * Parse the tokens as separated by comma and surrounded by whitespace.
 * Returns TRUE if found or FALSE if not.
 */
static bool contains_trailers(const char *p, size_t len)
{
  const char *end = p + len;
  for(;;) {
    for(; p != end && (*p == ' ' || *p == '\t'); ++p)
      ;
    if(p == end || (size_t)(end - p) < sizeof("trailers") - 1)
      return FALSE;
    if(strncasecompare("trailers", p, sizeof("trailers") - 1)) {
      p += sizeof("trailers") - 1;
      for(; p != end && (*p == ' ' || *p == '\t'); ++p)
        ;
      if(p == end || *p == ',')
        return TRUE;
    }
    /* skip to next token */
    for(; p != end && *p != ','; ++p)
      ;
    if(p == end)
      return FALSE;
    ++p;
  }
}

typedef enum {
  /* Send header to server */
  HEADERINST_FORWARD,
  /* Don't send header to server */
  HEADERINST_IGNORE,
  /* Discard header, and replace it with "te: trailers" */
  HEADERINST_TE_TRAILERS
} header_instruction;

/* Decides how to treat given header field. */
static header_instruction inspect_header(const char *name, size_t namelen,
                                         const char *value, size_t valuelen) {
  switch(namelen) {
  case 2:
    if(!strncasecompare("te", name, namelen))
      return HEADERINST_FORWARD;

    return contains_trailers(value, valuelen) ?
           HEADERINST_TE_TRAILERS : HEADERINST_IGNORE;
  case 7:
    return strncasecompare("upgrade", name, namelen) ?
           HEADERINST_IGNORE : HEADERINST_FORWARD;
  case 10:
    return (strncasecompare("connection", name, namelen) ||
            strncasecompare("keep-alive", name, namelen)) ?
           HEADERINST_IGNORE : HEADERINST_FORWARD;
  case 16:
    return strncasecompare("proxy-connection", name, namelen) ?
           HEADERINST_IGNORE : HEADERINST_FORWARD;
  case 17:
    return strncasecompare("transfer-encoding", name, namelen) ?
           HEADERINST_IGNORE : HEADERINST_FORWARD;
  default:
    return HEADERINST_FORWARD;
  }
}

static ssize_t http2_send(struct connectdata *conn, int sockindex,
                          const void *mem, size_t len, CURLcode *err)
{
  /*
   * BIG TODO: Currently, we send request in this function, but this
   * function is also used to send request body. It would be nice to
   * add dedicated function for request.
   */
  int rv;
  struct http_conn *httpc = &conn->proto.httpc;
  struct HTTP *stream = conn->data->req.protop;
  nghttp2_nv *nva = NULL;
  size_t nheader;
  size_t i;
  size_t authority_idx;
  char *hdbuf = (char *)mem;
  char *end, *line_end;
  nghttp2_data_provider data_prd;
  int32_t stream_id;
  nghttp2_session *h2 = httpc->h2;
  nghttp2_priority_spec pri_spec;

  (void)sockindex;

  H2BUGF(infof(conn->data, "http2_send len=%zu\n", len));

  if(stream->stream_id != -1) {
    if(stream->close_handled) {
      infof(conn->data, "stream %d closed\n", stream->stream_id);
      *err = CURLE_HTTP2_STREAM;
      return -1;
    }
    else if(stream->closed) {
      return http2_handle_stream_close(conn, conn->data, stream, err);
    }
    /* If stream_id != -1, we have dispatched request HEADERS, and now
       are going to send or sending request body in DATA frame */
    stream->upload_mem = mem;
    stream->upload_len = len;
    nghttp2_session_resume_data(h2, stream->stream_id);
    rv = h2_session_send(conn->data, h2);
    if(nghttp2_is_fatal(rv)) {
      *err = CURLE_SEND_ERROR;
      return -1;
    }
    len -= stream->upload_len;

    /* Nullify here because we call nghttp2_session_send() and they
       might refer to the old buffer. */
    stream->upload_mem = NULL;
    stream->upload_len = 0;

    if(should_close_session(httpc)) {
      H2BUGF(infof(conn->data, "http2_send: nothing to do in this session\n"));
      *err = CURLE_HTTP2;
      return -1;
    }

    if(stream->upload_left) {
      /* we are sure that we have more data to send here.  Calling the
         following API will make nghttp2_session_want_write() return
         nonzero if remote window allows it, which then libcurl checks
         socket is writable or not.  See http2_perform_getsock(). */
      nghttp2_session_resume_data(h2, stream->stream_id);
    }

    H2BUGF(infof(conn->data, "http2_send returns %zu for stream %u\n", len,
                 stream->stream_id));
    return len;
  }

  /* Calculate number of headers contained in [mem, mem + len) */
  /* Here, we assume the curl http code generate *correct* HTTP header
     field block */
  nheader = 0;
  for(i = 1; i < len; ++i) {
    if(hdbuf[i] == '\n' && hdbuf[i - 1] == '\r') {
      ++nheader;
      ++i;
    }
  }
  if(nheader < 2)
    goto fail;

  /* We counted additional 2 \r\n in the first and last line. We need 3
     new headers: :method, :path and :scheme. Therefore we need one
     more space. */
  nheader += 1;
  nva = malloc(sizeof(nghttp2_nv) * nheader);
  if(nva == NULL) {
    *err = CURLE_OUT_OF_MEMORY;
    return -1;
  }

  /* Extract :method, :path from request line
     We do line endings with CRLF so checking for CR is enough */
  line_end = memchr(hdbuf, '\r', len);
  if(!line_end)
    goto fail;

  /* Method does not contain spaces */
  end = memchr(hdbuf, ' ', line_end - hdbuf);
  if(!end || end == hdbuf)
    goto fail;
  nva[0].name = (unsigned char *)":method";
  nva[0].namelen = strlen((char *)nva[0].name);
  nva[0].value = (unsigned char *)hdbuf;
  nva[0].valuelen = (size_t)(end - hdbuf);
  nva[0].flags = NGHTTP2_NV_FLAG_NONE;
  if(HEADER_OVERFLOW(nva[0])) {
    failf(conn->data, "Failed sending HTTP request: Header overflow");
    goto fail;
  }

  hdbuf = end + 1;

  /* Path may contain spaces so scan backwards */
  end = NULL;
  for(i = (size_t)(line_end - hdbuf); i; --i) {
    if(hdbuf[i - 1] == ' ') {
      end = &hdbuf[i - 1];
      break;
    }
  }
  if(!end || end == hdbuf)
    goto fail;
  nva[1].name = (unsigned char *)":path";
  nva[1].namelen = strlen((char *)nva[1].name);
  nva[1].value = (unsigned char *)hdbuf;
  nva[1].valuelen = (size_t)(end - hdbuf);
  nva[1].flags = NGHTTP2_NV_FLAG_NONE;
  if(HEADER_OVERFLOW(nva[1])) {
    failf(conn->data, "Failed sending HTTP request: Header overflow");
    goto fail;
  }

  nva[2].name = (unsigned char *)":scheme";
  nva[2].namelen = strlen((char *)nva[2].name);
  if(conn->handler->flags & PROTOPT_SSL)
    nva[2].value = (unsigned char *)"https";
  else
    nva[2].value = (unsigned char *)"http";
  nva[2].valuelen = strlen((char *)nva[2].value);
  nva[2].flags = NGHTTP2_NV_FLAG_NONE;
  if(HEADER_OVERFLOW(nva[2])) {
    failf(conn->data, "Failed sending HTTP request: Header overflow");
    goto fail;
  }

  authority_idx = 0;
  i = 3;
  while(i < nheader) {
    size_t hlen;

    hdbuf = line_end + 2;

    /* check for next CR, but only within the piece of data left in the given
       buffer */
    line_end = memchr(hdbuf, '\r', len - (hdbuf - (char *)mem));
    if(!line_end || (line_end == hdbuf))
      goto fail;

    /* header continuation lines are not supported */
    if(*hdbuf == ' ' || *hdbuf == '\t')
      goto fail;

    for(end = hdbuf; end < line_end && *end != ':'; ++end)
      ;
    if(end == hdbuf || end == line_end)
      goto fail;
    hlen = end - hdbuf;

    if(hlen == 4 && strncasecompare("host", hdbuf, 4)) {
      authority_idx = i;
      nva[i].name = (unsigned char *)":authority";
      nva[i].namelen = strlen((char *)nva[i].name);
    }
    else {
      nva[i].name = (unsigned char *)hdbuf;
      nva[i].namelen = (size_t)(end - hdbuf);
    }
    hdbuf = end + 1;
    while(*hdbuf == ' ' || *hdbuf == '\t')
      ++hdbuf;
    end = line_end;

    switch(inspect_header((const char *)nva[i].name, nva[i].namelen, hdbuf,
                          end - hdbuf)) {
    case HEADERINST_IGNORE:
      /* skip header fields prohibited by HTTP/2 specification. */
      --nheader;
      continue;
    case HEADERINST_TE_TRAILERS:
      nva[i].value = (uint8_t*)"trailers";
      nva[i].valuelen = sizeof("trailers") - 1;
      break;
    default:
      nva[i].value = (unsigned char *)hdbuf;
      nva[i].valuelen = (size_t)(end - hdbuf);
    }

    nva[i].flags = NGHTTP2_NV_FLAG_NONE;
    if(HEADER_OVERFLOW(nva[i])) {
      failf(conn->data, "Failed sending HTTP request: Header overflow");
      goto fail;
    }
    ++i;
  }

  /* :authority must come before non-pseudo header fields */
  if(authority_idx != 0 && authority_idx != AUTHORITY_DST_IDX) {
    nghttp2_nv authority = nva[authority_idx];
    for(i = authority_idx; i > AUTHORITY_DST_IDX; --i) {
      nva[i] = nva[i - 1];
    }
    nva[i] = authority;
  }

  /* Warn stream may be rejected if cumulative length of headers is too large.
     It appears nghttp2 will not send a header frame larger than 64KB. */
#define MAX_ACC 60000  /* <64KB to account for some overhead */
  {
    size_t acc = 0;

    for(i = 0; i < nheader; ++i) {
      acc += nva[i].namelen + nva[i].valuelen;

      H2BUGF(infof(conn->data, "h2 header: %.*s:%.*s\n",
                   nva[i].namelen, nva[i].name,
                   nva[i].valuelen, nva[i].value));
    }

    if(acc > MAX_ACC) {
      infof(conn->data, "http2_send: Warning: The cumulative length of all "
            "headers exceeds %zu bytes and that could cause the "
            "stream to be rejected.\n", MAX_ACC);
    }
  }

  h2_pri_spec(conn->data, &pri_spec);

  switch(conn->data->set.httpreq) {
  case HTTPREQ_POST:
  case HTTPREQ_POST_FORM:
  case HTTPREQ_POST_MIME:
  case HTTPREQ_PUT:
    if(conn->data->state.infilesize != -1)
      stream->upload_left = conn->data->state.infilesize;
    else
      /* data sending without specifying the data amount up front */
      stream->upload_left = -1; /* unknown, but not zero */

    data_prd.read_callback = data_source_read_callback;
    data_prd.source.ptr = NULL;
    stream_id = nghttp2_submit_request(h2, &pri_spec, nva, nheader,
                                       &data_prd, conn->data);
    break;
  default:
    stream_id = nghttp2_submit_request(h2, &pri_spec, nva, nheader,
                                       NULL, conn->data);
  }

  Curl_safefree(nva);

  if(stream_id < 0) {
    H2BUGF(infof(conn->data, "http2_send() send error\n"));
    *err = CURLE_SEND_ERROR;
    return -1;
  }

  infof(conn->data, "Using Stream ID: %x (easy handle %p)\n",
        stream_id, (void *)conn->data);
  stream->stream_id = stream_id;

  /* this does not call h2_session_send() since there can not have been any
   * priority upodate since the nghttp2_submit_request() call above */
  rv = nghttp2_session_send(h2);

  if(rv != 0) {
    *err = CURLE_SEND_ERROR;
    return -1;
  }

  if(should_close_session(httpc)) {
    H2BUGF(infof(conn->data, "http2_send: nothing to do in this session\n"));
    *err = CURLE_HTTP2;
    return -1;
  }

  if(stream->stream_id != -1) {
    /* If whole HEADERS frame was sent off to the underlying socket,
       the nghttp2 library calls data_source_read_callback. But only
       it found that no data available, so it deferred the DATA
       transmission. Which means that nghttp2_session_want_write()
       returns 0 on http2_perform_getsock(), which results that no
       writable socket check is performed. To workaround this, we
       issue nghttp2_session_resume_data() here to bring back DATA
       transmission from deferred state. */
    nghttp2_session_resume_data(h2, stream->stream_id);
  }

  return len;

fail:
  free(nva);
  *err = CURLE_SEND_ERROR;
  return -1;
}

CURLcode Curl_http2_setup(struct connectdata *conn)
{
  CURLcode result;
  struct http_conn *httpc = &conn->proto.httpc;
  struct HTTP *stream = conn->data->req.protop;

  stream->stream_id = -1;

  if(!stream->header_recvbuf) {
    stream->header_recvbuf = Curl_add_buffer_init();
    if(!stream->header_recvbuf)
      return CURLE_OUT_OF_MEMORY;
  }

  if((conn->handler == &Curl_handler_http2_ssl) ||
     (conn->handler == &Curl_handler_http2))
    return CURLE_OK; /* already done */

  if(conn->handler->flags & PROTOPT_SSL)
    conn->handler = &Curl_handler_http2_ssl;
  else
    conn->handler = &Curl_handler_http2;

  result = Curl_http2_init(conn);
  if(result) {
    Curl_add_buffer_free(&stream->header_recvbuf);
    return result;
  }

  infof(conn->data, "Using HTTP2, server supports multi-use\n");
  stream->upload_left = 0;
  stream->upload_mem = NULL;
  stream->upload_len = 0;

  httpc->inbuflen = 0;
  httpc->nread_inbuf = 0;

  httpc->pause_stream_id = 0;
  httpc->drain_total = 0;

  conn->bits.multiplex = TRUE; /* at least potentially multiplexed */
  conn->httpversion = 20;
  conn->bundle->multiuse = BUNDLE_MULTIPLEX;

  infof(conn->data, "Connection state changed (HTTP/2 confirmed)\n");
  Curl_multi_connchanged(conn->data->multi);

  return CURLE_OK;
}

CURLcode Curl_http2_switched(struct connectdata *conn,
                             const char *mem, size_t nread)
{
  CURLcode result;
  struct http_conn *httpc = &conn->proto.httpc;
  int rv;
  ssize_t nproc;
  struct Curl_easy *data = conn->data;
  struct HTTP *stream = conn->data->req.protop;

  result = Curl_http2_setup(conn);
  if(result)
    return result;

  httpc->recv_underlying = conn->recv[FIRSTSOCKET];
  httpc->send_underlying = conn->send[FIRSTSOCKET];
  conn->recv[FIRSTSOCKET] = http2_recv;
  conn->send[FIRSTSOCKET] = http2_send;

  if(conn->data->req.upgr101 == UPGR101_RECEIVED) {
    /* stream 1 is opened implicitly on upgrade */
    stream->stream_id = 1;
    /* queue SETTINGS frame (again) */
    rv = nghttp2_session_upgrade(httpc->h2, httpc->binsettings,
                                 httpc->binlen, NULL);
    if(rv != 0) {
      failf(data, "nghttp2_session_upgrade() failed: %s(%d)",
            nghttp2_strerror(rv), rv);
      return CURLE_HTTP2;
    }

    rv = nghttp2_session_set_stream_user_data(httpc->h2,
                                              stream->stream_id,
                                              data);
    if(rv) {
      infof(data, "http/2: failed to set user_data for stream %d!\n",
            stream->stream_id);
      DEBUGASSERT(0);
    }
  }
  else {
    populate_settings(conn, httpc);

    /* stream ID is unknown at this point */
    stream->stream_id = -1;
    rv = nghttp2_submit_settings(httpc->h2, NGHTTP2_FLAG_NONE,
                                 httpc->local_settings,
                                 httpc->local_settings_num);
    if(rv != 0) {
      failf(data, "nghttp2_submit_settings() failed: %s(%d)",
            nghttp2_strerror(rv), rv);
      return CURLE_HTTP2;
    }
  }

#ifdef NGHTTP2_HAS_SET_LOCAL_WINDOW_SIZE
  rv = nghttp2_session_set_local_window_size(httpc->h2, NGHTTP2_FLAG_NONE, 0,
                                             HTTP2_HUGE_WINDOW_SIZE);
  if(rv != 0) {
    failf(data, "nghttp2_session_set_local_window_size() failed: %s(%d)",
          nghttp2_strerror(rv), rv);
    return CURLE_HTTP2;
  }
#endif

  /* we are going to copy mem to httpc->inbuf.  This is required since
     mem is part of buffer pointed by stream->mem, and callbacks
     called by nghttp2_session_mem_recv() will write stream specific
     data into stream->mem, overwriting data already there. */
  if(H2_BUFSIZE < nread) {
    failf(data, "connection buffer size is too small to store data following "
                "HTTP Upgrade response header: buflen=%zu, datalen=%zu",
          H2_BUFSIZE, nread);
    return CURLE_HTTP2;
  }

  infof(conn->data, "Copying HTTP/2 data in stream buffer to connection buffer"
                    " after upgrade: len=%zu\n",
        nread);

  if(nread)
    memcpy(httpc->inbuf, mem, nread);
  httpc->inbuflen = nread;

  nproc = nghttp2_session_mem_recv(httpc->h2, (const uint8_t *)httpc->inbuf,
                                   httpc->inbuflen);

  if(nghttp2_is_fatal((int)nproc)) {
    failf(data, "nghttp2_session_mem_recv() failed: %s(%d)",
          nghttp2_strerror((int)nproc), (int)nproc);
    return CURLE_HTTP2;
  }

  H2BUGF(infof(data, "nghttp2_session_mem_recv() returns %zd\n", nproc));

  if((ssize_t)nread == nproc) {
    httpc->inbuflen = 0;
    httpc->nread_inbuf = 0;
  }
  else {
    httpc->nread_inbuf += nproc;
  }

  /* Try to send some frames since we may read SETTINGS already. */
  rv = h2_session_send(data, httpc->h2);

  if(rv != 0) {
    failf(data, "nghttp2_session_send() failed: %s(%d)",
          nghttp2_strerror(rv), rv);
    return CURLE_HTTP2;
  }

  if(should_close_session(httpc)) {
    H2BUGF(infof(data,
                 "nghttp2_session_send(): nothing to do in this session\n"));
    return CURLE_HTTP2;
  }

  return CURLE_OK;
}

CURLcode Curl_http2_add_child(struct Curl_easy *parent,
                              struct Curl_easy *child,
                              bool exclusive)
{
  if(parent) {
    struct Curl_http2_dep **tail;
    struct Curl_http2_dep *dep = calloc(1, sizeof(struct Curl_http2_dep));
    if(!dep)
      return CURLE_OUT_OF_MEMORY;
    dep->data = child;

    if(parent->set.stream_dependents && exclusive) {
      struct Curl_http2_dep *node = parent->set.stream_dependents;
      while(node) {
        node->data->set.stream_depends_on = child;
        node = node->next;
      }

      tail = &child->set.stream_dependents;
      while(*tail)
        tail = &(*tail)->next;

      DEBUGASSERT(!*tail);
      *tail = parent->set.stream_dependents;
      parent->set.stream_dependents = 0;
    }

    tail = &parent->set.stream_dependents;
    while(*tail) {
      (*tail)->data->set.stream_depends_e = FALSE;
      tail = &(*tail)->next;
    }

    DEBUGASSERT(!*tail);
    *tail = dep;
  }

  child->set.stream_depends_on = parent;
  child->set.stream_depends_e = exclusive;
  return CURLE_OK;
}

void Curl_http2_remove_child(struct Curl_easy *parent, struct Curl_easy *child)
{
  struct Curl_http2_dep *last = 0;
  struct Curl_http2_dep *data = parent->set.stream_dependents;
  DEBUGASSERT(child->set.stream_depends_on == parent);

  while(data && data->data != child) {
    last = data;
    data = data->next;
  }

  DEBUGASSERT(data);

  if(data) {
    if(last) {
      last->next = data->next;
    }
    else {
      parent->set.stream_dependents = data->next;
    }
    free(data);
  }

  child->set.stream_depends_on = 0;
  child->set.stream_depends_e = FALSE;
}

void Curl_http2_cleanup_dependencies(struct Curl_easy *data)
{
  while(data->set.stream_dependents) {
    struct Curl_easy *tmp = data->set.stream_dependents->data;
    Curl_http2_remove_child(data, tmp);
    if(data->set.stream_depends_on)
      Curl_http2_add_child(data->set.stream_depends_on, tmp, FALSE);
  }

  if(data->set.stream_depends_on)
    Curl_http2_remove_child(data->set.stream_depends_on, data);
}

/* Only call this function for a transfer that already got a HTTP/2
   CURLE_HTTP2_STREAM error! */
bool Curl_h2_http_1_1_error(struct connectdata *conn)
{
  struct http_conn *httpc = &conn->proto.httpc;
  return (httpc->error_code == NGHTTP2_HTTP_1_1_REQUIRED);
}

#else /* !USE_NGHTTP2 */

/* Satisfy external references even if http2 is not compiled in. */

#define CURL_DISABLE_TYPECHECK
#include <curl/curl.h>

char *curl_pushheader_bynum(struct curl_pushheaders *h, size_t num)
{
  (void) h;
  (void) num;
  return NULL;
}

char *curl_pushheader_byname(struct curl_pushheaders *h, const char *header)
{
  (void) h;
  (void) header;
  return NULL;
}

#endif /* USE_NGHTTP2 */
