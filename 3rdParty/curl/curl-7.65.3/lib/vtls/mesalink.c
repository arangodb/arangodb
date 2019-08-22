/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2017 - 2018, Yiming Jing, <jingyiming@baidu.com>
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

/*
 * Source file for all MesaLink-specific code for the TLS/SSL layer. No code
 * but vtls.c should ever call or use these functions.
 *
 */

/*
 * Based upon the CyaSSL implementation in cyassl.c and cyassl.h:
 *   Copyright (C) 1998 - 2017, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * Thanks for code and inspiration!
 */

#include "curl_setup.h"

#ifdef USE_MESALINK

#include <mesalink/options.h>
#include <mesalink/version.h>

#include "urldata.h"
#include "sendf.h"
#include "inet_pton.h"
#include "vtls.h"
#include "parsedate.h"
#include "connect.h" /* for the connect timeout */
#include "select.h"
#include "strcase.h"
#include "x509asn1.h"
#include "curl_printf.h"

#include "mesalink.h"
#include <mesalink/openssl/ssl.h>
#include <mesalink/openssl/err.h>

/* The last #include files should be: */
#include "curl_memory.h"
#include "memdebug.h"

#define MESALINK_MAX_ERROR_SZ 80

struct ssl_backend_data
{
  SSL_CTX *ctx;
  SSL *handle;
};

#define BACKEND connssl->backend

static Curl_recv mesalink_recv;
static Curl_send mesalink_send;

/*
 * This function loads all the client/CA certificates and CRLs. Setup the TLS
 * layer and do all necessary magic.
 */
static CURLcode
mesalink_connect_step1(struct connectdata *conn, int sockindex)
{
  char *ciphers;
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  const bool verifypeer = SSL_CONN_CONFIG(verifypeer);
  const char *const ssl_cafile = SSL_CONN_CONFIG(CAfile);
  const char *const ssl_capath = SSL_CONN_CONFIG(CApath);
  struct in_addr addr4;
#ifdef ENABLE_IPV6
  struct in6_addr addr6;
#endif
  const char *const hostname =
    SSL_IS_PROXY() ? conn->http_proxy.host.name : conn->host.name;
  size_t hostname_len = strlen(hostname);

  SSL_METHOD *req_method = NULL;
  curl_socket_t sockfd = conn->sock[sockindex];

  if(connssl->state == ssl_connection_complete)
    return CURLE_OK;

  if(SSL_CONN_CONFIG(version_max) != CURL_SSLVERSION_MAX_NONE) {
    failf(data, "MesaLink does not support to set maximum SSL/TLS version");
    return CURLE_SSL_CONNECT_ERROR;
  }

  switch(SSL_CONN_CONFIG(version)) {
  case CURL_SSLVERSION_SSLv3:
  case CURL_SSLVERSION_TLSv1:
  case CURL_SSLVERSION_TLSv1_0:
  case CURL_SSLVERSION_TLSv1_1:
    failf(data, "MesaLink does not support SSL 3.0, TLS 1.0, or TLS 1.1");
    return CURLE_NOT_BUILT_IN;
  case CURL_SSLVERSION_DEFAULT:
  case CURL_SSLVERSION_TLSv1_2:
    req_method = TLSv1_2_client_method();
    break;
  case CURL_SSLVERSION_TLSv1_3:
    req_method = TLSv1_3_client_method();
    break;
  case CURL_SSLVERSION_SSLv2:
    failf(data, "MesaLink does not support SSLv2");
    return CURLE_SSL_CONNECT_ERROR;
  default:
    failf(data, "Unrecognized parameter passed via CURLOPT_SSLVERSION");
    return CURLE_SSL_CONNECT_ERROR;
  }

  if(!req_method) {
    failf(data, "SSL: couldn't create a method!");
    return CURLE_OUT_OF_MEMORY;
  }

  if(BACKEND->ctx)
    SSL_CTX_free(BACKEND->ctx);
  BACKEND->ctx = SSL_CTX_new(req_method);

  if(!BACKEND->ctx) {
    failf(data, "SSL: couldn't create a context!");
    return CURLE_OUT_OF_MEMORY;
  }

  SSL_CTX_set_verify(
    BACKEND->ctx, verifypeer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);

  if(ssl_cafile || ssl_capath) {
    if(!SSL_CTX_load_verify_locations(BACKEND->ctx, ssl_cafile, ssl_capath)) {
      if(verifypeer) {
        failf(data,
              "error setting certificate verify locations:\n"
              "  CAfile: %s\n  CApath: %s",
              ssl_cafile ? ssl_cafile : "none",
              ssl_capath ? ssl_capath : "none");
        return CURLE_SSL_CACERT_BADFILE;
      }
      infof(data,
            "error setting certificate verify locations,"
            " continuing anyway:\n");
    }
    else {
      infof(data, "successfully set certificate verify locations:\n");
    }
    infof(data,
          "  CAfile: %s\n"
          "  CApath: %s\n",
          ssl_cafile ? ssl_cafile : "none",
          ssl_capath ? ssl_capath : "none");
  }

  ciphers = SSL_CONN_CONFIG(cipher_list);
  if(ciphers) {
#ifdef MESALINK_HAVE_CIPHER
    if(!SSL_CTX_set_cipher_list(BACKEND->ctx, ciphers)) {
      failf(data, "failed setting cipher list: %s", ciphers);
      return CURLE_SSL_CIPHER;
    }
#endif
    infof(data, "Cipher selection: %s\n", ciphers);
  }

  if(BACKEND->handle)
    SSL_free(BACKEND->handle);
  BACKEND->handle = SSL_new(BACKEND->ctx);
  if(!BACKEND->handle) {
    failf(data, "SSL: couldn't create a context (handle)!");
    return CURLE_OUT_OF_MEMORY;
  }

  if((hostname_len < USHRT_MAX) &&
     (0 == Curl_inet_pton(AF_INET, hostname, &addr4))
#ifdef ENABLE_IPV6
     && (0 == Curl_inet_pton(AF_INET6, hostname, &addr6))
#endif
  ) {
    /* hostname is not a valid IP address */
    if(SSL_set_tlsext_host_name(BACKEND->handle, hostname) != SSL_SUCCESS) {
      failf(data,
            "WARNING: failed to configure server name indication (SNI) "
            "TLS extension\n");
      return CURLE_SSL_CONNECT_ERROR;
    }
  }
  else {
#ifdef CURLDEBUG
    /* Check if the hostname is 127.0.0.1 or [::1];
     * otherwise reject because MesaLink always wants a valid DNS Name
     * specified in RFC 5280 Section 7.2 */
    if(strncmp(hostname, "127.0.0.1", 9) == 0
#ifdef ENABLE_IPV6
       || strncmp(hostname, "[::1]", 5) == 0
#endif
    ) {
      SSL_set_tlsext_host_name(BACKEND->handle, "localhost");
    }
    else
#endif
    {
      failf(data,
            "ERROR: MesaLink does not accept an IP address as a hostname\n");
      return CURLE_SSL_CONNECT_ERROR;
    }
  }

#ifdef MESALINK_HAVE_SESSION
  if(SSL_SET_OPTION(primary.sessionid)) {
    void *ssl_sessionid = NULL;

    Curl_ssl_sessionid_lock(conn);
    if(!Curl_ssl_getsessionid(conn, &ssl_sessionid, NULL, sockindex)) {
      /* we got a session id, use it! */
      if(!SSL_set_session(BACKEND->handle, ssl_sessionid)) {
        Curl_ssl_sessionid_unlock(conn);
        failf(
          data,
          "SSL: SSL_set_session failed: %s",
          ERR_error_string(SSL_get_error(BACKEND->handle, 0), error_buffer));
        return CURLE_SSL_CONNECT_ERROR;
      }
      /* Informational message */
      infof(data, "SSL re-using session ID\n");
    }
    Curl_ssl_sessionid_unlock(conn);
  }
#endif /* MESALINK_HAVE_SESSION */

  if(SSL_set_fd(BACKEND->handle, (int)sockfd) != SSL_SUCCESS) {
    failf(data, "SSL: SSL_set_fd failed");
    return CURLE_SSL_CONNECT_ERROR;
  }

  connssl->connecting_state = ssl_connect_2;
  return CURLE_OK;
}

static CURLcode
mesalink_connect_step2(struct connectdata *conn, int sockindex)
{
  int ret = -1;
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];

  conn->recv[sockindex] = mesalink_recv;
  conn->send[sockindex] = mesalink_send;

  ret = SSL_connect(BACKEND->handle);
  if(ret != SSL_SUCCESS) {
    int detail = SSL_get_error(BACKEND->handle, ret);

    if(SSL_ERROR_WANT_CONNECT == detail || SSL_ERROR_WANT_READ == detail) {
      connssl->connecting_state = ssl_connect_2_reading;
      return CURLE_OK;
    }
    else {
      char error_buffer[MESALINK_MAX_ERROR_SZ];
      failf(data,
            "SSL_connect failed with error %d: %s",
            detail,
            ERR_error_string_n(detail, error_buffer, sizeof(error_buffer)));
      ERR_print_errors_fp(stderr);
      if(detail && SSL_CONN_CONFIG(verifypeer)) {
        detail &= ~0xFF;
        if(detail == TLS_ERROR_WEBPKI_ERRORS) {
          failf(data, "Cert verify failed");
          return CURLE_PEER_FAILED_VERIFICATION;
        }
      }
      return CURLE_SSL_CONNECT_ERROR;
    }
  }

  connssl->connecting_state = ssl_connect_3;
  infof(data,
        "SSL connection using %s / %s\n",
        SSL_get_version(BACKEND->handle),
        SSL_get_cipher_name(BACKEND->handle));

  return CURLE_OK;
}

static CURLcode
mesalink_connect_step3(struct connectdata *conn, int sockindex)
{
  CURLcode result = CURLE_OK;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];

  DEBUGASSERT(ssl_connect_3 == connssl->connecting_state);

#ifdef MESALINK_HAVE_SESSION
  if(SSL_SET_OPTION(primary.sessionid)) {
    bool incache;
    SSL_SESSION *our_ssl_sessionid;
    void *old_ssl_sessionid = NULL;

    our_ssl_sessionid = SSL_get_session(BACKEND->handle);

    Curl_ssl_sessionid_lock(conn);
    incache =
      !(Curl_ssl_getsessionid(conn, &old_ssl_sessionid, NULL, sockindex));
    if(incache) {
      if(old_ssl_sessionid != our_ssl_sessionid) {
        infof(data, "old SSL session ID is stale, removing\n");
        Curl_ssl_delsessionid(conn, old_ssl_sessionid);
        incache = FALSE;
      }
    }

    if(!incache) {
      result = Curl_ssl_addsessionid(
        conn, our_ssl_sessionid, 0 /* unknown size */, sockindex);
      if(result) {
        Curl_ssl_sessionid_unlock(conn);
        failf(data, "failed to store ssl session");
        return result;
      }
    }
    Curl_ssl_sessionid_unlock(conn);
  }
#endif /* MESALINK_HAVE_SESSION */

  connssl->connecting_state = ssl_connect_done;

  return result;
}

static ssize_t
mesalink_send(struct connectdata *conn, int sockindex, const void *mem,
              size_t len, CURLcode *curlcode)
{
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  char error_buffer[MESALINK_MAX_ERROR_SZ];
  int memlen = (len > (size_t)INT_MAX) ? INT_MAX : (int)len;
  int rc = SSL_write(BACKEND->handle, mem, memlen);

  if(rc < 0) {
    int err = SSL_get_error(BACKEND->handle, rc);
    switch(err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      /* there's data pending, re-invoke SSL_write() */
      *curlcode = CURLE_AGAIN;
      return -1;
    default:
      failf(conn->data,
            "SSL write: %s, errno %d",
            ERR_error_string_n(err, error_buffer, sizeof(error_buffer)),
            SOCKERRNO);
      *curlcode = CURLE_SEND_ERROR;
      return -1;
    }
  }
  return rc;
}

static void
Curl_mesalink_close(struct connectdata *conn, int sockindex)
{
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];

  if(BACKEND->handle) {
    (void)SSL_shutdown(BACKEND->handle);
    SSL_free(BACKEND->handle);
    BACKEND->handle = NULL;
  }
  if(BACKEND->ctx) {
    SSL_CTX_free(BACKEND->ctx);
    BACKEND->ctx = NULL;
  }
}

static ssize_t
mesalink_recv(struct connectdata *conn, int num, char *buf, size_t buffersize,
              CURLcode *curlcode)
{
  struct ssl_connect_data *connssl = &conn->ssl[num];
  char error_buffer[MESALINK_MAX_ERROR_SZ];
  int buffsize = (buffersize > (size_t)INT_MAX) ? INT_MAX : (int)buffersize;
  int nread = SSL_read(BACKEND->handle, buf, buffsize);

  if(nread <= 0) {
    int err = SSL_get_error(BACKEND->handle, nread);

    switch(err) {
    case SSL_ERROR_ZERO_RETURN: /* no more data */
    case IO_ERROR_CONNECTION_ABORTED:
      break;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      /* there's data pending, re-invoke SSL_read() */
      *curlcode = CURLE_AGAIN;
      return -1;
    default:
      failf(conn->data,
            "SSL read: %s, errno %d",
            ERR_error_string_n(err, error_buffer, sizeof(error_buffer)),
            SOCKERRNO);
      *curlcode = CURLE_RECV_ERROR;
      return -1;
    }
  }
  return nread;
}

static size_t
Curl_mesalink_version(char *buffer, size_t size)
{
  return msnprintf(buffer, size, "MesaLink/%s", MESALINK_VERSION_STRING);
}

static int
Curl_mesalink_init(void)
{
  return (SSL_library_init() == SSL_SUCCESS);
}

/*
 * This function is called to shut down the SSL layer but keep the
 * socket open (CCC - Clear Command Channel)
 */
static int
Curl_mesalink_shutdown(struct connectdata *conn, int sockindex)
{
  int retval = 0;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];

  if(BACKEND->handle) {
    SSL_free(BACKEND->handle);
    BACKEND->handle = NULL;
  }
  return retval;
}

static CURLcode
mesalink_connect_common(struct connectdata *conn, int sockindex,
                        bool nonblocking, bool *done)
{
  CURLcode result;
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  curl_socket_t sockfd = conn->sock[sockindex];
  time_t timeout_ms;
  int what;

  /* check if the connection has already been established */
  if(ssl_connection_complete == connssl->state) {
    *done = TRUE;
    return CURLE_OK;
  }

  if(ssl_connect_1 == connssl->connecting_state) {
    /* Find out how much more time we're allowed */
    timeout_ms = Curl_timeleft(data, NULL, TRUE);

    if(timeout_ms < 0) {
      /* no need to continue if time already is up */
      failf(data, "SSL connection timeout");
      return CURLE_OPERATION_TIMEDOUT;
    }

    result = mesalink_connect_step1(conn, sockindex);
    if(result)
      return result;
  }

  while(ssl_connect_2 == connssl->connecting_state ||
        ssl_connect_2_reading == connssl->connecting_state ||
        ssl_connect_2_writing == connssl->connecting_state) {

    /* check allowed time left */
    timeout_ms = Curl_timeleft(data, NULL, TRUE);

    if(timeout_ms < 0) {
      /* no need to continue if time already is up */
      failf(data, "SSL connection timeout");
      return CURLE_OPERATION_TIMEDOUT;
    }

    /* if ssl is expecting something, check if it's available. */
    if(connssl->connecting_state == ssl_connect_2_reading ||
       connssl->connecting_state == ssl_connect_2_writing) {

      curl_socket_t writefd =
        ssl_connect_2_writing == connssl->connecting_state ? sockfd
                                                           : CURL_SOCKET_BAD;
      curl_socket_t readfd = ssl_connect_2_reading == connssl->connecting_state
                               ? sockfd
                               : CURL_SOCKET_BAD;

      what = Curl_socket_check(
        readfd, CURL_SOCKET_BAD, writefd, nonblocking ? 0 : timeout_ms);
      if(what < 0) {
        /* fatal error */
        failf(data, "select/poll on SSL socket, errno: %d", SOCKERRNO);
        return CURLE_SSL_CONNECT_ERROR;
      }
      else if(0 == what) {
        if(nonblocking) {
          *done = FALSE;
          return CURLE_OK;
        }
        else {
          /* timeout */
          failf(data, "SSL connection timeout");
          return CURLE_OPERATION_TIMEDOUT;
        }
      }
      /* socket is readable or writable */
    }

    /* Run transaction, and return to the caller if it failed or if
     * this connection is part of a multi handle and this loop would
     * execute again. This permits the owner of a multi handle to
     * abort a connection attempt before step2 has completed while
     * ensuring that a client using select() or epoll() will always
     * have a valid fdset to wait on.
     */
    result = mesalink_connect_step2(conn, sockindex);

    if(result ||
       (nonblocking && (ssl_connect_2 == connssl->connecting_state ||
                        ssl_connect_2_reading == connssl->connecting_state ||
                        ssl_connect_2_writing == connssl->connecting_state))) {
      return result;
    }
  } /* repeat step2 until all transactions are done. */

  if(ssl_connect_3 == connssl->connecting_state) {
    result = mesalink_connect_step3(conn, sockindex);
    if(result)
      return result;
  }

  if(ssl_connect_done == connssl->connecting_state) {
    connssl->state = ssl_connection_complete;
    conn->recv[sockindex] = mesalink_recv;
    conn->send[sockindex] = mesalink_send;
    *done = TRUE;
  }
  else
    *done = FALSE;

  /* Reset our connect state machine */
  connssl->connecting_state = ssl_connect_1;

  return CURLE_OK;
}

static CURLcode
Curl_mesalink_connect_nonblocking(struct connectdata *conn, int sockindex,
                                  bool *done)
{
  return mesalink_connect_common(conn, sockindex, TRUE, done);
}

static CURLcode
Curl_mesalink_connect(struct connectdata *conn, int sockindex)
{
  CURLcode result;
  bool done = FALSE;

  result = mesalink_connect_common(conn, sockindex, FALSE, &done);
  if(result)
    return result;

  DEBUGASSERT(done);

  return CURLE_OK;
}

static void *
Curl_mesalink_get_internals(struct ssl_connect_data *connssl,
                            CURLINFO info UNUSED_PARAM)
{
  (void)info;
  return BACKEND->handle;
}

const struct Curl_ssl Curl_ssl_mesalink = {
  { CURLSSLBACKEND_MESALINK, "MesaLink" }, /* info */

  SSLSUPP_SSL_CTX,

  sizeof(struct ssl_backend_data),

  Curl_mesalink_init, /* init */
  Curl_none_cleanup, /* cleanup */
  Curl_mesalink_version, /* version */
  Curl_none_check_cxn, /* check_cxn */
  Curl_mesalink_shutdown, /* shutdown */
  Curl_none_data_pending, /* data_pending */
  Curl_none_random, /* random */
  Curl_none_cert_status_request, /* cert_status_request */
  Curl_mesalink_connect, /* connect */
  Curl_mesalink_connect_nonblocking, /* connect_nonblocking */
  Curl_mesalink_get_internals, /* get_internals */
  Curl_mesalink_close, /* close_one */
  Curl_none_close_all, /* close_all */
  Curl_none_session_free, /* session_free */
  Curl_none_set_engine, /* set_engine */
  Curl_none_set_engine_default, /* set_engine_default */
  Curl_none_engines_list, /* engines_list */
  Curl_none_false_start, /* false_start */
  Curl_none_md5sum, /* md5sum */
  NULL /* sha256sum */
};

#endif
