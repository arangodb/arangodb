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

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_LINUX_TCP_H
#include <linux/tcp.h>
#endif

#include <curl/curl.h>

#include "urldata.h"
#include "sendf.h"
#include "connect.h"
#include "vtls/vtls.h"
#include "ssh.h"
#include "easyif.h"
#include "multiif.h"
#include "non-ascii.h"
#include "strerror.h"
#include "select.h"
#include "strdup.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#ifdef CURL_DO_LINEEND_CONV
/*
 * convert_lineends() changes CRLF (\r\n) end-of-line markers to a single LF
 * (\n), with special processing for CRLF sequences that are split between two
 * blocks of data.  Remaining, bare CRs are changed to LFs.  The possibly new
 * size of the data is returned.
 */
static size_t convert_lineends(struct Curl_easy *data,
                               char *startPtr, size_t size)
{
  char *inPtr, *outPtr;

  /* sanity check */
  if((startPtr == NULL) || (size < 1)) {
    return size;
  }

  if(data->state.prev_block_had_trailing_cr) {
    /* The previous block of incoming data
       had a trailing CR, which was turned into a LF. */
    if(*startPtr == '\n') {
      /* This block of incoming data starts with the
         previous block's LF so get rid of it */
      memmove(startPtr, startPtr + 1, size-1);
      size--;
      /* and it wasn't a bare CR but a CRLF conversion instead */
      data->state.crlf_conversions++;
    }
    data->state.prev_block_had_trailing_cr = FALSE; /* reset the flag */
  }

  /* find 1st CR, if any */
  inPtr = outPtr = memchr(startPtr, '\r', size);
  if(inPtr) {
    /* at least one CR, now look for CRLF */
    while(inPtr < (startPtr + size-1)) {
      /* note that it's size-1, so we'll never look past the last byte */
      if(memcmp(inPtr, "\r\n", 2) == 0) {
        /* CRLF found, bump past the CR and copy the NL */
        inPtr++;
        *outPtr = *inPtr;
        /* keep track of how many CRLFs we converted */
        data->state.crlf_conversions++;
      }
      else {
        if(*inPtr == '\r') {
          /* lone CR, move LF instead */
          *outPtr = '\n';
        }
        else {
          /* not a CRLF nor a CR, just copy whatever it is */
          *outPtr = *inPtr;
        }
      }
      outPtr++;
      inPtr++;
    } /* end of while loop */

    if(inPtr < startPtr + size) {
      /* handle last byte */
      if(*inPtr == '\r') {
        /* deal with a CR at the end of the buffer */
        *outPtr = '\n'; /* copy a NL instead */
        /* note that a CRLF might be split across two blocks */
        data->state.prev_block_had_trailing_cr = TRUE;
      }
      else {
        /* copy last byte */
        *outPtr = *inPtr;
      }
      outPtr++;
    }
    if(outPtr < startPtr + size)
      /* tidy up by null terminating the now shorter data */
      *outPtr = '\0';

    return (outPtr - startPtr);
  }
  return size;
}
#endif /* CURL_DO_LINEEND_CONV */

#ifdef USE_RECV_BEFORE_SEND_WORKAROUND
bool Curl_recv_has_postponed_data(struct connectdata *conn, int sockindex)
{
  struct postponed_data * const psnd = &(conn->postponed[sockindex]);
  return psnd->buffer && psnd->allocated_size &&
         psnd->recv_size > psnd->recv_processed;
}

static void pre_receive_plain(struct connectdata *conn, int num)
{
  const curl_socket_t sockfd = conn->sock[num];
  struct postponed_data * const psnd = &(conn->postponed[num]);
  size_t bytestorecv = psnd->allocated_size - psnd->recv_size;
  /* WinSock will destroy unread received data if send() is
     failed.
     To avoid lossage of received data, recv() must be
     performed before every send() if any incoming data is
     available. However, skip this, if buffer is already full. */
  if((conn->handler->protocol&PROTO_FAMILY_HTTP) != 0 &&
     conn->recv[num] == Curl_recv_plain &&
     (!psnd->buffer || bytestorecv)) {
    const int readymask = Curl_socket_check(sockfd, CURL_SOCKET_BAD,
                                            CURL_SOCKET_BAD, 0);
    if(readymask != -1 && (readymask & CURL_CSELECT_IN) != 0) {
      /* Have some incoming data */
      if(!psnd->buffer) {
        /* Use buffer double default size for intermediate buffer */
        psnd->allocated_size = 2 * conn->data->set.buffer_size;
        psnd->buffer = malloc(psnd->allocated_size);
        psnd->recv_size = 0;
        psnd->recv_processed = 0;
#ifdef DEBUGBUILD
        psnd->bindsock = sockfd; /* Used only for DEBUGASSERT */
#endif /* DEBUGBUILD */
        bytestorecv = psnd->allocated_size;
      }
      if(psnd->buffer) {
        ssize_t recvedbytes;
        DEBUGASSERT(psnd->bindsock == sockfd);
        recvedbytes = sread(sockfd, psnd->buffer + psnd->recv_size,
                            bytestorecv);
        if(recvedbytes > 0)
          psnd->recv_size += recvedbytes;
      }
      else
        psnd->allocated_size = 0;
    }
  }
}

static ssize_t get_pre_recved(struct connectdata *conn, int num, char *buf,
                              size_t len)
{
  struct postponed_data * const psnd = &(conn->postponed[num]);
  size_t copysize;
  if(!psnd->buffer)
    return 0;

  DEBUGASSERT(psnd->allocated_size > 0);
  DEBUGASSERT(psnd->recv_size <= psnd->allocated_size);
  DEBUGASSERT(psnd->recv_processed <= psnd->recv_size);
  /* Check and process data that already received and storied in internal
     intermediate buffer */
  if(psnd->recv_size > psnd->recv_processed) {
    DEBUGASSERT(psnd->bindsock == conn->sock[num]);
    copysize = CURLMIN(len, psnd->recv_size - psnd->recv_processed);
    memcpy(buf, psnd->buffer + psnd->recv_processed, copysize);
    psnd->recv_processed += copysize;
  }
  else
    copysize = 0; /* buffer was allocated, but nothing was received */

  /* Free intermediate buffer if it has no unprocessed data */
  if(psnd->recv_processed == psnd->recv_size) {
    free(psnd->buffer);
    psnd->buffer = NULL;
    psnd->allocated_size = 0;
    psnd->recv_size = 0;
    psnd->recv_processed = 0;
#ifdef DEBUGBUILD
    psnd->bindsock = CURL_SOCKET_BAD;
#endif /* DEBUGBUILD */
  }
  return (ssize_t)copysize;
}
#else  /* ! USE_RECV_BEFORE_SEND_WORKAROUND */
/* Use "do-nothing" macros instead of functions when workaround not used */
bool Curl_recv_has_postponed_data(struct connectdata *conn, int sockindex)
{
  (void)conn;
  (void)sockindex;
  return false;
}
#define pre_receive_plain(c,n) do {} WHILE_FALSE
#define get_pre_recved(c,n,b,l) 0
#endif /* ! USE_RECV_BEFORE_SEND_WORKAROUND */

/* Curl_infof() is for info message along the way */

void Curl_infof(struct Curl_easy *data, const char *fmt, ...)
{
  if(data && data->set.verbose) {
    va_list ap;
    size_t len;
    char print_buffer[2048 + 1];
    va_start(ap, fmt);
    len = mvsnprintf(print_buffer, sizeof(print_buffer), fmt, ap);
    /*
     * Indicate truncation of the input by replacing the last 3 characters
     * with "...", and transfer the newline over in case the format had one.
     */
    if(len >= sizeof(print_buffer)) {
      len = strlen(fmt);
      if(fmt[--len] == '\n')
        msnprintf(print_buffer + (sizeof(print_buffer) - 5), 5, "...\n");
      else
        msnprintf(print_buffer + (sizeof(print_buffer) - 4), 4, "...");
    }
    va_end(ap);
    len = strlen(print_buffer);
    Curl_debug(data, CURLINFO_TEXT, print_buffer, len);
  }
}

/* Curl_failf() is for messages stating why we failed.
 * The message SHALL NOT include any LF or CR.
 */

void Curl_failf(struct Curl_easy *data, const char *fmt, ...)
{
  if(data->set.verbose || data->set.errorbuffer) {
    va_list ap;
    size_t len;
    char error[CURL_ERROR_SIZE + 2];
    va_start(ap, fmt);
    mvsnprintf(error, CURL_ERROR_SIZE, fmt, ap);
    len = strlen(error);

    if(data->set.errorbuffer && !data->state.errorbuf) {
      strcpy(data->set.errorbuffer, error);
      data->state.errorbuf = TRUE; /* wrote error string */
    }
    if(data->set.verbose) {
      error[len] = '\n';
      error[++len] = '\0';
      Curl_debug(data, CURLINFO_TEXT, error, len);
    }
    va_end(ap);
  }
}

/* Curl_sendf() sends formatted data to the server */
CURLcode Curl_sendf(curl_socket_t sockfd, struct connectdata *conn,
                    const char *fmt, ...)
{
  struct Curl_easy *data = conn->data;
  ssize_t bytes_written;
  size_t write_len;
  CURLcode result = CURLE_OK;
  char *s;
  char *sptr;
  va_list ap;
  va_start(ap, fmt);
  s = vaprintf(fmt, ap); /* returns an allocated string */
  va_end(ap);
  if(!s)
    return CURLE_OUT_OF_MEMORY; /* failure */

  bytes_written = 0;
  write_len = strlen(s);
  sptr = s;

  for(;;) {
    /* Write the buffer to the socket */
    result = Curl_write(conn, sockfd, sptr, write_len, &bytes_written);

    if(result)
      break;

    if(data->set.verbose)
      Curl_debug(data, CURLINFO_DATA_OUT, sptr, (size_t)bytes_written);

    if((size_t)bytes_written != write_len) {
      /* if not all was written at once, we must advance the pointer, decrease
         the size left and try again! */
      write_len -= bytes_written;
      sptr += bytes_written;
    }
    else
      break;
  }

  free(s); /* free the output string */

  return result;
}

/*
 * Curl_write() is an internal write function that sends data to the
 * server. Works with plain sockets, SCP, SSL or kerberos.
 *
 * If the write would block (CURLE_AGAIN), we return CURLE_OK and
 * (*written == 0). Otherwise we return regular CURLcode value.
 */
CURLcode Curl_write(struct connectdata *conn,
                    curl_socket_t sockfd,
                    const void *mem,
                    size_t len,
                    ssize_t *written)
{
  ssize_t bytes_written;
  CURLcode result = CURLE_OK;
  int num = (sockfd == conn->sock[SECONDARYSOCKET]);

  bytes_written = conn->send[num](conn, num, mem, len, &result);

  *written = bytes_written;
  if(bytes_written >= 0)
    /* we completely ignore the curlcode value when subzero is not returned */
    return CURLE_OK;

  /* handle CURLE_AGAIN or a send failure */
  switch(result) {
  case CURLE_AGAIN:
    *written = 0;
    return CURLE_OK;

  case CURLE_OK:
    /* general send failure */
    return CURLE_SEND_ERROR;

  default:
    /* we got a specific curlcode, forward it */
    return result;
  }
}

ssize_t Curl_send_plain(struct connectdata *conn, int num,
                        const void *mem, size_t len, CURLcode *code)
{
  curl_socket_t sockfd = conn->sock[num];
  ssize_t bytes_written;
  /* WinSock will destroy unread received data if send() is
     failed.
     To avoid lossage of received data, recv() must be
     performed before every send() if any incoming data is
     available. */
  pre_receive_plain(conn, num);

#if defined(MSG_FASTOPEN) && !defined(TCP_FASTOPEN_CONNECT) /* Linux */
  if(conn->bits.tcp_fastopen) {
    bytes_written = sendto(sockfd, mem, len, MSG_FASTOPEN,
                           conn->ip_addr->ai_addr, conn->ip_addr->ai_addrlen);
    conn->bits.tcp_fastopen = FALSE;
  }
  else
#endif
    bytes_written = swrite(sockfd, mem, len);

  *code = CURLE_OK;
  if(-1 == bytes_written) {
    int err = SOCKERRNO;

    if(
#ifdef WSAEWOULDBLOCK
      /* This is how Windows does it */
      (WSAEWOULDBLOCK == err)
#else
      /* errno may be EWOULDBLOCK or on some systems EAGAIN when it returned
         due to its inability to send off data without blocking. We therefore
         treat both error codes the same here */
      (EWOULDBLOCK == err) || (EAGAIN == err) || (EINTR == err) ||
      (EINPROGRESS == err)
#endif
      ) {
      /* this is just a case of EWOULDBLOCK */
      bytes_written = 0;
      *code = CURLE_AGAIN;
    }
    else {
      failf(conn->data, "Send failure: %s",
            Curl_strerror(conn, err));
      conn->data->state.os_errno = err;
      *code = CURLE_SEND_ERROR;
    }
  }
  return bytes_written;
}

/*
 * Curl_write_plain() is an internal write function that sends data to the
 * server using plain sockets only. Otherwise meant to have the exact same
 * proto as Curl_write()
 */
CURLcode Curl_write_plain(struct connectdata *conn,
                          curl_socket_t sockfd,
                          const void *mem,
                          size_t len,
                          ssize_t *written)
{
  ssize_t bytes_written;
  CURLcode result;
  int num = (sockfd == conn->sock[SECONDARYSOCKET]);

  bytes_written = Curl_send_plain(conn, num, mem, len, &result);

  *written = bytes_written;

  return result;
}

ssize_t Curl_recv_plain(struct connectdata *conn, int num, char *buf,
                        size_t len, CURLcode *code)
{
  curl_socket_t sockfd = conn->sock[num];
  ssize_t nread;
  /* Check and return data that already received and storied in internal
     intermediate buffer */
  nread = get_pre_recved(conn, num, buf, len);
  if(nread > 0) {
    *code = CURLE_OK;
    return nread;
  }

  nread = sread(sockfd, buf, len);

  *code = CURLE_OK;
  if(-1 == nread) {
    int err = SOCKERRNO;

    if(
#ifdef WSAEWOULDBLOCK
      /* This is how Windows does it */
      (WSAEWOULDBLOCK == err)
#else
      /* errno may be EWOULDBLOCK or on some systems EAGAIN when it returned
         due to its inability to send off data without blocking. We therefore
         treat both error codes the same here */
      (EWOULDBLOCK == err) || (EAGAIN == err) || (EINTR == err)
#endif
      ) {
      /* this is just a case of EWOULDBLOCK */
      *code = CURLE_AGAIN;
    }
    else {
      failf(conn->data, "Recv failure: %s",
            Curl_strerror(conn, err));
      conn->data->state.os_errno = err;
      *code = CURLE_RECV_ERROR;
    }
  }
  return nread;
}

static CURLcode pausewrite(struct Curl_easy *data,
                           int type, /* what type of data */
                           const char *ptr,
                           size_t len)
{
  /* signalled to pause sending on this connection, but since we have data
     we want to send we need to dup it to save a copy for when the sending
     is again enabled */
  struct SingleRequest *k = &data->req;
  struct UrlState *s = &data->state;
  char *dupl;
  unsigned int i;
  bool newtype = TRUE;

  if(s->tempcount) {
    for(i = 0; i< s->tempcount; i++) {
      if(s->tempwrite[i].type == type) {
        /* data for this type exists */
        newtype = FALSE;
        break;
      }
    }
    DEBUGASSERT(i < 3);
  }
  else
    i = 0;

  if(!newtype) {
    /* append new data to old data */

    /* figure out the new size of the data to save */
    size_t newlen = len + s->tempwrite[i].len;
    /* allocate the new memory area */
    char *newptr = realloc(s->tempwrite[i].buf, newlen);
    if(!newptr)
      return CURLE_OUT_OF_MEMORY;
    /* copy the new data to the end of the new area */
    memcpy(newptr + s->tempwrite[i].len, ptr, len);

    /* update the pointer and the size */
    s->tempwrite[i].buf = newptr;
    s->tempwrite[i].len = newlen;
  }
  else {
    dupl = Curl_memdup(ptr, len);
    if(!dupl)
      return CURLE_OUT_OF_MEMORY;

    /* store this information in the state struct for later use */
    s->tempwrite[i].buf = dupl;
    s->tempwrite[i].len = len;
    s->tempwrite[i].type = type;

    if(newtype)
      s->tempcount++;
  }

  /* mark the connection as RECV paused */
  k->keepon |= KEEP_RECV_PAUSE;

  DEBUGF(infof(data, "Paused %zu bytes in buffer for type %02x\n",
               len, type));

  return CURLE_OK;
}


/* chop_write() writes chunks of data not larger than CURL_MAX_WRITE_SIZE via
 * client write callback(s) and takes care of pause requests from the
 * callbacks.
 */
static CURLcode chop_write(struct connectdata *conn,
                           int type,
                           char *optr,
                           size_t olen)
{
  struct Curl_easy *data = conn->data;
  curl_write_callback writeheader = NULL;
  curl_write_callback writebody = NULL;
  char *ptr = optr;
  size_t len = olen;

  if(!len)
    return CURLE_OK;

  /* If reading is paused, append this data to the already held data for this
     type. */
  if(data->req.keepon & KEEP_RECV_PAUSE)
    return pausewrite(data, type, ptr, len);

  /* Determine the callback(s) to use. */
  if(type & CLIENTWRITE_BODY)
    writebody = data->set.fwrite_func;
  if((type & CLIENTWRITE_HEADER) &&
     (data->set.fwrite_header || data->set.writeheader)) {
    /*
     * Write headers to the same callback or to the especially setup
     * header callback function (added after version 7.7.1).
     */
    writeheader =
      data->set.fwrite_header? data->set.fwrite_header: data->set.fwrite_func;
  }

  /* Chop data, write chunks. */
  while(len) {
    size_t chunklen = len <= CURL_MAX_WRITE_SIZE? len: CURL_MAX_WRITE_SIZE;

    if(writebody) {
      size_t wrote = writebody(ptr, 1, chunklen, data->set.out);

      if(CURL_WRITEFUNC_PAUSE == wrote) {
        if(conn->handler->flags & PROTOPT_NONETWORK) {
          /* Protocols that work without network cannot be paused. This is
             actually only FILE:// just now, and it can't pause since the
             transfer isn't done using the "normal" procedure. */
          failf(data, "Write callback asked for PAUSE when not supported!");
          return CURLE_WRITE_ERROR;
        }
        return pausewrite(data, type, ptr, len);
      }
      if(wrote != chunklen) {
        failf(data, "Failed writing body (%zu != %zu)", wrote, chunklen);
        return CURLE_WRITE_ERROR;
      }
    }

    ptr += chunklen;
    len -= chunklen;
  }

  if(writeheader) {
    size_t wrote;
    ptr = optr;
    len = olen;
    Curl_set_in_callback(data, true);
    wrote = writeheader(ptr, 1, len, data->set.writeheader);
    Curl_set_in_callback(data, false);

    if(CURL_WRITEFUNC_PAUSE == wrote)
      /* here we pass in the HEADER bit only since if this was body as well
         then it was passed already and clearly that didn't trigger the
         pause, so this is saved for later with the HEADER bit only */
      return pausewrite(data, CLIENTWRITE_HEADER, ptr, len);

    if(wrote != len) {
      failf(data, "Failed writing header");
      return CURLE_WRITE_ERROR;
    }
  }

  return CURLE_OK;
}


/* Curl_client_write() sends data to the write callback(s)

   The bit pattern defines to what "streams" to write to. Body and/or header.
   The defines are in sendf.h of course.

   If CURL_DO_LINEEND_CONV is enabled, data is converted IN PLACE to the
   local character encoding.  This is a problem and should be changed in
   the future to leave the original data alone.
 */
CURLcode Curl_client_write(struct connectdata *conn,
                           int type,
                           char *ptr,
                           size_t len)
{
  struct Curl_easy *data = conn->data;

  if(0 == len)
    len = strlen(ptr);

  DEBUGASSERT(type <= 3);

  /* FTP data may need conversion. */
  if((type & CLIENTWRITE_BODY) &&
    (conn->handler->protocol & PROTO_FAMILY_FTP) &&
    conn->proto.ftpc.transfertype == 'A') {
    /* convert from the network encoding */
    CURLcode result = Curl_convert_from_network(data, ptr, len);
    /* Curl_convert_from_network calls failf if unsuccessful */
    if(result)
      return result;

#ifdef CURL_DO_LINEEND_CONV
    /* convert end-of-line markers */
    len = convert_lineends(data, ptr, len);
#endif /* CURL_DO_LINEEND_CONV */
    }

  return chop_write(conn, type, ptr, len);
}

CURLcode Curl_read_plain(curl_socket_t sockfd,
                         char *buf,
                         size_t bytesfromsocket,
                         ssize_t *n)
{
  ssize_t nread = sread(sockfd, buf, bytesfromsocket);

  if(-1 == nread) {
    int err = SOCKERRNO;
    int return_error;
#ifdef USE_WINSOCK
    return_error = WSAEWOULDBLOCK == err;
#else
    return_error = EWOULDBLOCK == err || EAGAIN == err || EINTR == err;
#endif
    if(return_error)
      return CURLE_AGAIN;
    return CURLE_RECV_ERROR;
  }

  /* we only return number of bytes read when we return OK */
  *n = nread;
  return CURLE_OK;
}

/*
 * Internal read-from-socket function. This is meant to deal with plain
 * sockets, SSL sockets and kerberos sockets.
 *
 * Returns a regular CURLcode value.
 */
CURLcode Curl_read(struct connectdata *conn, /* connection data */
                   curl_socket_t sockfd,     /* read from this socket */
                   char *buf,                /* store read data here */
                   size_t sizerequested,     /* max amount to read */
                   ssize_t *n)               /* amount bytes read */
{
  CURLcode result = CURLE_RECV_ERROR;
  ssize_t nread = 0;
  size_t bytesfromsocket = 0;
  char *buffertofill = NULL;
  struct Curl_easy *data = conn->data;

  /* if HTTP/1 pipelining is both wanted and possible */
  bool pipelining = Curl_pipeline_wanted(data->multi, CURLPIPE_HTTP1) &&
    (conn->bundle->multiuse == BUNDLE_PIPELINING);

  /* Set 'num' to 0 or 1, depending on which socket that has been sent here.
     If it is the second socket, we set num to 1. Otherwise to 0. This lets
     us use the correct ssl handle. */
  int num = (sockfd == conn->sock[SECONDARYSOCKET]);

  *n = 0; /* reset amount to zero */

  /* If session can pipeline, check connection buffer  */
  if(pipelining) {
    size_t bytestocopy = CURLMIN(conn->buf_len - conn->read_pos,
                                 sizerequested);

    /* Copy from our master buffer first if we have some unread data there*/
    if(bytestocopy > 0) {
      memcpy(buf, conn->master_buffer + conn->read_pos, bytestocopy);
      conn->read_pos += bytestocopy;
      conn->bits.stream_was_rewound = FALSE;

      *n = (ssize_t)bytestocopy;
      return CURLE_OK;
    }
    /* If we come here, it means that there is no data to read from the buffer,
     * so we read from the socket */
    bytesfromsocket = CURLMIN(sizerequested, MASTERBUF_SIZE);
    buffertofill = conn->master_buffer;
  }
  else {
    bytesfromsocket = CURLMIN(sizerequested, (size_t)data->set.buffer_size);
    buffertofill = buf;
  }

  nread = conn->recv[num](conn, num, buffertofill, bytesfromsocket, &result);
  if(nread < 0)
    return result;

  if(pipelining) {
    memcpy(buf, conn->master_buffer, nread);
    conn->buf_len = nread;
    conn->read_pos = nread;
  }

  *n += nread;

  return CURLE_OK;
}

/* return 0 on success */
int Curl_debug(struct Curl_easy *data, curl_infotype type,
               char *ptr, size_t size)
{
  static const char s_infotype[CURLINFO_END][3] = {
    "* ", "< ", "> ", "{ ", "} ", "{ ", "} " };
  int rc = 0;

#ifdef CURL_DOES_CONVERSIONS
  char *buf = NULL;
  size_t conv_size = 0;

  switch(type) {
  case CURLINFO_HEADER_OUT:
    buf = Curl_memdup(ptr, size);
    if(!buf)
      return 1;
    conv_size = size;

    /* Special processing is needed for this block if it
     * contains both headers and data (separated by CRLFCRLF).
     * We want to convert just the headers, leaving the data as-is.
     */
    if(size > 4) {
      size_t i;
      for(i = 0; i < size-4; i++) {
        if(memcmp(&buf[i], "\x0d\x0a\x0d\x0a", 4) == 0) {
          /* convert everything through this CRLFCRLF but no further */
          conv_size = i + 4;
          break;
        }
      }
    }

    Curl_convert_from_network(data, buf, conv_size);
    /* Curl_convert_from_network calls failf if unsuccessful */
    /* we might as well continue even if it fails...   */
    ptr = buf; /* switch pointer to use my buffer instead */
    break;
  default:
    /* leave everything else as-is */
    break;
  }
#endif /* CURL_DOES_CONVERSIONS */

  if(data->set.fdebug) {
    Curl_set_in_callback(data, true);
    rc = (*data->set.fdebug)(data, type, ptr, size, data->set.debugdata);
    Curl_set_in_callback(data, false);
  }
  else {
    switch(type) {
    case CURLINFO_TEXT:
    case CURLINFO_HEADER_OUT:
    case CURLINFO_HEADER_IN:
      fwrite(s_infotype[type], 2, 1, data->set.err);
      fwrite(ptr, size, 1, data->set.err);
#ifdef CURL_DOES_CONVERSIONS
      if(size != conv_size) {
        /* we had untranslated data so we need an explicit newline */
        fwrite("\n", 1, 1, data->set.err);
      }
#endif
      break;
    default: /* nada */
      break;
    }
  }
#ifdef CURL_DOES_CONVERSIONS
  free(buf);
#endif
  return rc;
}
