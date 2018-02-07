/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2017, Daniel Stenberg, <daniel@haxx.se>, et al.
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
 * Source file for all GnuTLS-specific code for the TLS/SSL layer. No code
 * but vtls.c should ever call or use these functions.
 *
 * Note: don't use the GnuTLS' *_t variable type names in this source code,
 * since they were not present in 1.0.X.
 */

#include "curl_setup.h"

#ifdef USE_GNUTLS

#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#ifdef USE_GNUTLS_NETTLE
#include <gnutls/crypto.h>
#include <nettle/md5.h>
#include <nettle/sha2.h>
#else
#include <gcrypt.h>
#endif

#include "urldata.h"
#include "sendf.h"
#include "inet_pton.h"
#include "gtls.h"
#include "vtls.h"
#include "parsedate.h"
#include "connect.h" /* for the connect timeout */
#include "select.h"
#include "strcase.h"
#include "warnless.h"
#include "x509asn1.h"
#include "curl_printf.h"
#include "curl_memory.h"
/* The last #include file should be: */
#include "memdebug.h"

#ifndef GNUTLS_POINTER_TO_SOCKET_CAST
#define GNUTLS_POINTER_TO_SOCKET_CAST(p) \
  ((curl_socket_t) ((char *)(p) - (char *)NULL))
#endif
#ifndef GNUTLS_SOCKET_TO_POINTER_CAST
#define GNUTLS_SOCKET_TO_POINTER_CAST(s) \
  ((void *) ((char *)NULL + (s)))
#endif

/* Enable GnuTLS debugging by defining GTLSDEBUG */
/*#define GTLSDEBUG */

#ifdef GTLSDEBUG
static void tls_log_func(int level, const char *str)
{
    fprintf(stderr, "|<%d>| %s", level, str);
}
#endif
static bool gtls_inited = FALSE;

#if defined(GNUTLS_VERSION_NUMBER)
#  if (GNUTLS_VERSION_NUMBER >= 0x020c00)
#    undef gnutls_transport_set_lowat
#    define gnutls_transport_set_lowat(A,B) Curl_nop_stmt
#    define USE_GNUTLS_PRIORITY_SET_DIRECT 1
#  endif
#  if (GNUTLS_VERSION_NUMBER >= 0x020c03)
#    define GNUTLS_MAPS_WINSOCK_ERRORS 1
#  endif

#  if HAVE_GNUTLS_ALPN_SET_PROTOCOLS
#    define HAS_ALPN
#  endif

#  if HAVE_GNUTLS_OCSP_REQ_INIT
#    define HAS_OCSP
#  endif

#  if (GNUTLS_VERSION_NUMBER >= 0x030306)
#    define HAS_CAPATH
#  endif
#endif

#ifdef HAS_OCSP
# include <gnutls/ocsp.h>
#endif

struct ssl_backend_data {
  gnutls_session_t session;
  gnutls_certificate_credentials_t cred;
#ifdef USE_TLS_SRP
  gnutls_srp_client_credentials_t srp_client_cred;
#endif
};

#define BACKEND connssl->backend

/*
 * Custom push and pull callback functions used by GNU TLS to read and write
 * to the socket.  These functions are simple wrappers to send() and recv()
 * (although here using the sread/swrite macros as defined by
 * curl_setup_once.h).
 * We use custom functions rather than the GNU TLS defaults because it allows
 * us to get specific about the fourth "flags" argument, and to use arbitrary
 * private data with gnutls_transport_set_ptr if we wish.
 *
 * When these custom push and pull callbacks fail, GNU TLS checks its own
 * session-specific error variable, and when not set also its own global
 * errno variable, in order to take appropriate action. GNU TLS does not
 * require that the transport is actually a socket. This implies that for
 * Windows builds these callbacks should ideally set the session-specific
 * error variable using function gnutls_transport_set_errno or as a last
 * resort global errno variable using gnutls_transport_set_global_errno,
 * with a transport agnostic error value. This implies that some winsock
 * error translation must take place in these callbacks.
 *
 * Paragraph above applies to GNU TLS versions older than 2.12.3, since
 * this version GNU TLS does its own internal winsock error translation
 * using system_errno() function.
 */

#if defined(USE_WINSOCK) && !defined(GNUTLS_MAPS_WINSOCK_ERRORS)
#  define gtls_EINTR  4
#  define gtls_EIO    5
#  define gtls_EAGAIN 11
static int gtls_mapped_sockerrno(void)
{
  switch(SOCKERRNO) {
  case WSAEWOULDBLOCK:
    return gtls_EAGAIN;
  case WSAEINTR:
    return gtls_EINTR;
  default:
    break;
  }
  return gtls_EIO;
}
#endif

static ssize_t Curl_gtls_push(void *s, const void *buf, size_t len)
{
  ssize_t ret = swrite(GNUTLS_POINTER_TO_SOCKET_CAST(s), buf, len);
#if defined(USE_WINSOCK) && !defined(GNUTLS_MAPS_WINSOCK_ERRORS)
  if(ret < 0)
    gnutls_transport_set_global_errno(gtls_mapped_sockerrno());
#endif
  return ret;
}

static ssize_t Curl_gtls_pull(void *s, void *buf, size_t len)
{
  ssize_t ret = sread(GNUTLS_POINTER_TO_SOCKET_CAST(s), buf, len);
#if defined(USE_WINSOCK) && !defined(GNUTLS_MAPS_WINSOCK_ERRORS)
  if(ret < 0)
    gnutls_transport_set_global_errno(gtls_mapped_sockerrno());
#endif
  return ret;
}

static ssize_t Curl_gtls_push_ssl(void *s, const void *buf, size_t len)
{
  return gnutls_record_send((gnutls_session_t) s, buf, len);
}

static ssize_t Curl_gtls_pull_ssl(void *s, void *buf, size_t len)
{
  return gnutls_record_recv((gnutls_session_t) s, buf, len);
}

/* Curl_gtls_init()
 *
 * Global GnuTLS init, called from Curl_ssl_init(). This calls functions that
 * are not thread-safe and thus this function itself is not thread-safe and
 * must only be called from within curl_global_init() to keep the thread
 * situation under control!
 */
static int Curl_gtls_init(void)
{
  int ret = 1;
  if(!gtls_inited) {
    ret = gnutls_global_init()?0:1;
#ifdef GTLSDEBUG
    gnutls_global_set_log_function(tls_log_func);
    gnutls_global_set_log_level(2);
#endif
    gtls_inited = TRUE;
  }
  return ret;
}

static void Curl_gtls_cleanup(void)
{
  if(gtls_inited) {
    gnutls_global_deinit();
    gtls_inited = FALSE;
  }
}

#ifndef CURL_DISABLE_VERBOSE_STRINGS
static void showtime(struct Curl_easy *data,
                     const char *text,
                     time_t stamp)
{
  struct tm buffer;
  const struct tm *tm = &buffer;
  char str[96];
  CURLcode result = Curl_gmtime(stamp, &buffer);
  if(result)
    return;

  snprintf(str,
           sizeof(str),
           "\t %s: %s, %02d %s %4d %02d:%02d:%02d GMT",
           text,
           Curl_wkday[tm->tm_wday?tm->tm_wday-1:6],
           tm->tm_mday,
           Curl_month[tm->tm_mon],
           tm->tm_year + 1900,
           tm->tm_hour,
           tm->tm_min,
           tm->tm_sec);
  infof(data, "%s\n", str);
}
#endif

static gnutls_datum_t load_file(const char *file)
{
  FILE *f;
  gnutls_datum_t loaded_file = { NULL, 0 };
  long filelen;
  void *ptr;

  f = fopen(file, "rb");
  if(!f)
    return loaded_file;
  if(fseek(f, 0, SEEK_END) != 0
     || (filelen = ftell(f)) < 0
     || fseek(f, 0, SEEK_SET) != 0
     || !(ptr = malloc((size_t)filelen)))
    goto out;
  if(fread(ptr, 1, (size_t)filelen, f) < (size_t)filelen) {
    free(ptr);
    goto out;
  }

  loaded_file.data = ptr;
  loaded_file.size = (unsigned int)filelen;
out:
  fclose(f);
  return loaded_file;
}

static void unload_file(gnutls_datum_t data)
{
  free(data.data);
}


/* this function does a SSL/TLS (re-)handshake */
static CURLcode handshake(struct connectdata *conn,
                          int sockindex,
                          bool duringconnect,
                          bool nonblocking)
{
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  gnutls_session_t session = BACKEND->session;
  curl_socket_t sockfd = conn->sock[sockindex];
  time_t timeout_ms;
  int rc;
  int what;

  for(;;) {
    /* check allowed time left */
    timeout_ms = Curl_timeleft(data, NULL, duringconnect);

    if(timeout_ms < 0) {
      /* no need to continue if time already is up */
      failf(data, "SSL connection timeout");
      return CURLE_OPERATION_TIMEDOUT;
    }

    /* if ssl is expecting something, check if it's available. */
    if(connssl->connecting_state == ssl_connect_2_reading
       || connssl->connecting_state == ssl_connect_2_writing) {

      curl_socket_t writefd = ssl_connect_2_writing ==
        connssl->connecting_state?sockfd:CURL_SOCKET_BAD;
      curl_socket_t readfd = ssl_connect_2_reading ==
        connssl->connecting_state?sockfd:CURL_SOCKET_BAD;

      what = Curl_socket_check(readfd, CURL_SOCKET_BAD, writefd,
                               nonblocking?0:
                               timeout_ms?timeout_ms:1000);
      if(what < 0) {
        /* fatal error */
        failf(data, "select/poll on SSL socket, errno: %d", SOCKERRNO);
        return CURLE_SSL_CONNECT_ERROR;
      }
      else if(0 == what) {
        if(nonblocking)
          return CURLE_OK;
        else if(timeout_ms) {
          /* timeout */
          failf(data, "SSL connection timeout at %ld", (long)timeout_ms);
          return CURLE_OPERATION_TIMEDOUT;
        }
      }
      /* socket is readable or writable */
    }

    rc = gnutls_handshake(session);

    if((rc == GNUTLS_E_AGAIN) || (rc == GNUTLS_E_INTERRUPTED)) {
      connssl->connecting_state =
        gnutls_record_get_direction(session)?
        ssl_connect_2_writing:ssl_connect_2_reading;
      continue;
    }
    else if((rc < 0) && !gnutls_error_is_fatal(rc)) {
      const char *strerr = NULL;

      if(rc == GNUTLS_E_WARNING_ALERT_RECEIVED) {
        int alert = gnutls_alert_get(session);
        strerr = gnutls_alert_get_name(alert);
      }

      if(strerr == NULL)
        strerr = gnutls_strerror(rc);

      infof(data, "gnutls_handshake() warning: %s\n", strerr);
      continue;
    }
    else if(rc < 0) {
      const char *strerr = NULL;

      if(rc == GNUTLS_E_FATAL_ALERT_RECEIVED) {
        int alert = gnutls_alert_get(session);
        strerr = gnutls_alert_get_name(alert);
      }

      if(strerr == NULL)
        strerr = gnutls_strerror(rc);

      failf(data, "gnutls_handshake() failed: %s", strerr);
      return CURLE_SSL_CONNECT_ERROR;
    }

    /* Reset our connect state machine */
    connssl->connecting_state = ssl_connect_1;
    return CURLE_OK;
  }
}

static gnutls_x509_crt_fmt_t do_file_type(const char *type)
{
  if(!type || !type[0])
    return GNUTLS_X509_FMT_PEM;
  if(strcasecompare(type, "PEM"))
    return GNUTLS_X509_FMT_PEM;
  if(strcasecompare(type, "DER"))
    return GNUTLS_X509_FMT_DER;
  return -1;
}

#ifndef USE_GNUTLS_PRIORITY_SET_DIRECT
static CURLcode
set_ssl_version_min_max(int *list, size_t list_size, struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  long ssl_version = SSL_CONN_CONFIG(version);
  long ssl_version_max = SSL_CONN_CONFIG(version_max);
  long i = ssl_version;
  long protocol_priority_idx = 0;

  switch(ssl_version_max) {
    case CURL_SSLVERSION_MAX_NONE:
      ssl_version_max = ssl_version << 16;
      break;
    case CURL_SSLVERSION_MAX_DEFAULT:
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_2;
      break;
  }

  for(; i <= (ssl_version_max >> 16) &&
        protocol_priority_idx < list_size; ++i) {
    switch(i) {
      case CURL_SSLVERSION_TLSv1_0:
        protocol_priority[protocol_priority_idx++] = GNUTLS_TLS1_0;
        break;
      case CURL_SSLVERSION_TLSv1_1:
        protocol_priority[protocol_priority_idx++] = GNUTLS_TLS1_1;
        break;
      case CURL_SSLVERSION_TLSv1_2:
        protocol_priority[protocol_priority_idx++] = GNUTLS_TLS1_2;
        break;
      case CURL_SSLVERSION_TLSv1_3:
        failf(data, "GnuTLS: TLS 1.3 is not yet supported");
        return CURLE_SSL_CONNECT_ERROR;
    }
  }
  return CURLE_OK;
}
#else
#define GNUTLS_CIPHERS "NORMAL:-ARCFOUR-128:-CTYPE-ALL:+CTYPE-X509"
/* If GnuTLS was compiled without support for SRP it will error out if SRP is
   requested in the priority string, so treat it specially
 */
#define GNUTLS_SRP "+SRP"

static CURLcode
set_ssl_version_min_max(const char **prioritylist, struct connectdata *conn)
{
  struct Curl_easy *data = conn->data;
  long ssl_version = SSL_CONN_CONFIG(version);
  long ssl_version_max = SSL_CONN_CONFIG(version_max);
  if(ssl_version == CURL_SSLVERSION_TLSv1_3 ||
     ssl_version_max == CURL_SSLVERSION_MAX_TLSv1_3) {
    failf(data, "GnuTLS: TLS 1.3 is not yet supported");
    return CURLE_SSL_CONNECT_ERROR;
  }
  if(ssl_version_max == CURL_SSLVERSION_MAX_NONE) {
    ssl_version_max = ssl_version << 16;
  }
  switch(ssl_version | ssl_version_max) {
    case CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_0:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.0:" GNUTLS_SRP;
      return CURLE_OK;
    case CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_1:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.0:+VERS-TLS1.1:" GNUTLS_SRP;
      return CURLE_OK;
    case CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_2:
    case CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_DEFAULT:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.0:+VERS-TLS1.1:+VERS-TLS1.2:" GNUTLS_SRP;
      return CURLE_OK;
    case CURL_SSLVERSION_TLSv1_1 | CURL_SSLVERSION_MAX_TLSv1_1:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.1:" GNUTLS_SRP;
      return CURLE_OK;
    case CURL_SSLVERSION_TLSv1_1 | CURL_SSLVERSION_MAX_TLSv1_2:
    case CURL_SSLVERSION_TLSv1_1 | CURL_SSLVERSION_MAX_DEFAULT:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.1:+VERS-TLS1.2:" GNUTLS_SRP;
      return CURLE_OK;
    case CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2:
    case CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_DEFAULT:
      *prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:-VERS-TLS-ALL:"
                      "+VERS-TLS1.2:" GNUTLS_SRP;
      return CURLE_OK;
  }

  failf(data, "GnuTLS: cannot set ssl protocol");
  return CURLE_SSL_CONNECT_ERROR;
}
#endif

static CURLcode
gtls_connect_step1(struct connectdata *conn,
                   int sockindex)
{
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  unsigned int init_flags;
  gnutls_session_t session;
  int rc;
  bool sni = TRUE; /* default is SNI enabled */
  void *transport_ptr = NULL;
  gnutls_push_func gnutls_transport_push = NULL;
  gnutls_pull_func gnutls_transport_pull = NULL;
#ifdef ENABLE_IPV6
  struct in6_addr addr;
#else
  struct in_addr addr;
#endif
#ifndef USE_GNUTLS_PRIORITY_SET_DIRECT
  static const int cipher_priority[] = {
  /* These two ciphers were added to GnuTLS as late as ver. 3.0.1,
     but this code path is only ever used for ver. < 2.12.0.
     GNUTLS_CIPHER_AES_128_GCM,
     GNUTLS_CIPHER_AES_256_GCM,
  */
    GNUTLS_CIPHER_AES_128_CBC,
    GNUTLS_CIPHER_AES_256_CBC,
    GNUTLS_CIPHER_CAMELLIA_128_CBC,
    GNUTLS_CIPHER_CAMELLIA_256_CBC,
    GNUTLS_CIPHER_3DES_CBC,
  };
  static const int cert_type_priority[] = { GNUTLS_CRT_X509, 0 };
  int protocol_priority[] = { 0, 0, 0, 0 };
#else
  const char *prioritylist;
  const char *err = NULL;
#endif

  const char * const hostname = SSL_IS_PROXY() ? conn->http_proxy.host.name :
    conn->host.name;

  if(connssl->state == ssl_connection_complete)
    /* to make us tolerant against being called more than once for the
       same connection */
    return CURLE_OK;

  if(!gtls_inited)
    Curl_gtls_init();

  if(SSL_CONN_CONFIG(version) == CURL_SSLVERSION_SSLv2) {
    failf(data, "GnuTLS does not support SSLv2");
    return CURLE_SSL_CONNECT_ERROR;
  }
  else if(SSL_CONN_CONFIG(version) == CURL_SSLVERSION_SSLv3)
    sni = FALSE; /* SSLv3 has no SNI */

  /* allocate a cred struct */
  rc = gnutls_certificate_allocate_credentials(&BACKEND->cred);
  if(rc != GNUTLS_E_SUCCESS) {
    failf(data, "gnutls_cert_all_cred() failed: %s", gnutls_strerror(rc));
    return CURLE_SSL_CONNECT_ERROR;
  }

#ifdef USE_TLS_SRP
  if(SSL_SET_OPTION(authtype) == CURL_TLSAUTH_SRP) {
    infof(data, "Using TLS-SRP username: %s\n", SSL_SET_OPTION(username));

    rc = gnutls_srp_allocate_client_credentials(
           &BACKEND->srp_client_cred);
    if(rc != GNUTLS_E_SUCCESS) {
      failf(data, "gnutls_srp_allocate_client_cred() failed: %s",
            gnutls_strerror(rc));
      return CURLE_OUT_OF_MEMORY;
    }

    rc = gnutls_srp_set_client_credentials(BACKEND->srp_client_cred,
                                           SSL_SET_OPTION(username),
                                           SSL_SET_OPTION(password));
    if(rc != GNUTLS_E_SUCCESS) {
      failf(data, "gnutls_srp_set_client_cred() failed: %s",
            gnutls_strerror(rc));
      return CURLE_BAD_FUNCTION_ARGUMENT;
    }
  }
#endif

  if(SSL_CONN_CONFIG(CAfile)) {
    /* set the trusted CA cert bundle file */
    gnutls_certificate_set_verify_flags(BACKEND->cred,
                                        GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    rc = gnutls_certificate_set_x509_trust_file(BACKEND->cred,
                                                SSL_CONN_CONFIG(CAfile),
                                                GNUTLS_X509_FMT_PEM);
    if(rc < 0) {
      infof(data, "error reading ca cert file %s (%s)\n",
            SSL_CONN_CONFIG(CAfile), gnutls_strerror(rc));
      if(SSL_CONN_CONFIG(verifypeer))
        return CURLE_SSL_CACERT_BADFILE;
    }
    else
      infof(data, "found %d certificates in %s\n", rc,
            SSL_CONN_CONFIG(CAfile));
  }

#ifdef HAS_CAPATH
  if(SSL_CONN_CONFIG(CApath)) {
    /* set the trusted CA cert directory */
    rc = gnutls_certificate_set_x509_trust_dir(BACKEND->cred,
                                               SSL_CONN_CONFIG(CApath),
                                               GNUTLS_X509_FMT_PEM);
    if(rc < 0) {
      infof(data, "error reading ca cert file %s (%s)\n",
            SSL_CONN_CONFIG(CApath), gnutls_strerror(rc));
      if(SSL_CONN_CONFIG(verifypeer))
        return CURLE_SSL_CACERT_BADFILE;
    }
    else
      infof(data, "found %d certificates in %s\n",
            rc, SSL_CONN_CONFIG(CApath));
  }
#endif

#ifdef CURL_CA_FALLBACK
  /* use system ca certificate store as fallback */
  if(SSL_CONN_CONFIG(verifypeer) &&
     !(SSL_CONN_CONFIG(CAfile) || SSL_CONN_CONFIG(CApath))) {
    gnutls_certificate_set_x509_system_trust(BACKEND->cred);
  }
#endif

  if(SSL_SET_OPTION(CRLfile)) {
    /* set the CRL list file */
    rc = gnutls_certificate_set_x509_crl_file(BACKEND->cred,
                                              SSL_SET_OPTION(CRLfile),
                                              GNUTLS_X509_FMT_PEM);
    if(rc < 0) {
      failf(data, "error reading crl file %s (%s)",
            SSL_SET_OPTION(CRLfile), gnutls_strerror(rc));
      return CURLE_SSL_CRL_BADFILE;
    }
    else
      infof(data, "found %d CRL in %s\n",
            rc, SSL_SET_OPTION(CRLfile));
  }

  /* Initialize TLS session as a client */
  init_flags = GNUTLS_CLIENT;

#if defined(GNUTLS_NO_TICKETS)
  /* Disable TLS session tickets */
  init_flags |= GNUTLS_NO_TICKETS;
#endif

  rc = gnutls_init(&BACKEND->session, init_flags);
  if(rc != GNUTLS_E_SUCCESS) {
    failf(data, "gnutls_init() failed: %d", rc);
    return CURLE_SSL_CONNECT_ERROR;
  }

  /* convenient assign */
  session = BACKEND->session;

  if((0 == Curl_inet_pton(AF_INET, hostname, &addr)) &&
#ifdef ENABLE_IPV6
     (0 == Curl_inet_pton(AF_INET6, hostname, &addr)) &&
#endif
     sni &&
     (gnutls_server_name_set(session, GNUTLS_NAME_DNS, hostname,
                             strlen(hostname)) < 0))
    infof(data, "WARNING: failed to configure server name indication (SNI) "
          "TLS extension\n");

  /* Use default priorities */
  rc = gnutls_set_default_priority(session);
  if(rc != GNUTLS_E_SUCCESS)
    return CURLE_SSL_CONNECT_ERROR;

#ifndef USE_GNUTLS_PRIORITY_SET_DIRECT
  rc = gnutls_cipher_set_priority(session, cipher_priority);
  if(rc != GNUTLS_E_SUCCESS)
    return CURLE_SSL_CONNECT_ERROR;

  /* Sets the priority on the certificate types supported by gnutls. Priority
   is higher for types specified before others. After specifying the types
   you want, you must append a 0. */
  rc = gnutls_certificate_type_set_priority(session, cert_type_priority);
  if(rc != GNUTLS_E_SUCCESS)
    return CURLE_SSL_CONNECT_ERROR;

  if(SSL_CONN_CONFIG(cipher_list) != NULL) {
    failf(data, "can't pass a custom cipher list to older GnuTLS"
          " versions");
    return CURLE_SSL_CONNECT_ERROR;
  }

  switch(SSL_CONN_CONFIG(version)) {
    case CURL_SSLVERSION_SSLv3:
      protocol_priority[0] = GNUTLS_SSL3;
      break;
    case CURL_SSLVERSION_DEFAULT:
    case CURL_SSLVERSION_TLSv1:
      protocol_priority[0] = GNUTLS_TLS1_0;
      protocol_priority[1] = GNUTLS_TLS1_1;
      protocol_priority[2] = GNUTLS_TLS1_2;
      break;
    case CURL_SSLVERSION_TLSv1_0:
    case CURL_SSLVERSION_TLSv1_1:
    case CURL_SSLVERSION_TLSv1_2:
    case CURL_SSLVERSION_TLSv1_3:
      {
        CURLcode result = set_ssl_version_min_max(protocol_priority,
                sizeof(protocol_priority)/sizeof(protocol_priority[0]), conn);
        if(result != CURLE_OK)
          return result;
        break;
      }
    case CURL_SSLVERSION_SSLv2:
      failf(data, "GnuTLS does not support SSLv2");
      return CURLE_SSL_CONNECT_ERROR;
    default:
      failf(data, "Unrecognized parameter passed via CURLOPT_SSLVERSION");
      return CURLE_SSL_CONNECT_ERROR;
  }
  rc = gnutls_protocol_set_priority(session, protocol_priority);
  if(rc != GNUTLS_E_SUCCESS) {
    failf(data, "Did you pass a valid GnuTLS cipher list?");
    return CURLE_SSL_CONNECT_ERROR;
  }

#else
  /* Ensure +SRP comes at the *end* of all relevant strings so that it can be
   * removed if a run-time error indicates that SRP is not supported by this
   * GnuTLS version */
  switch(SSL_CONN_CONFIG(version)) {
    case CURL_SSLVERSION_SSLv3:
      prioritylist = GNUTLS_CIPHERS ":-VERS-TLS-ALL:+VERS-SSL3.0";
      sni = false;
      break;
    case CURL_SSLVERSION_DEFAULT:
    case CURL_SSLVERSION_TLSv1:
      prioritylist = GNUTLS_CIPHERS ":-VERS-SSL3.0:" GNUTLS_SRP;
      break;
    case CURL_SSLVERSION_TLSv1_0:
    case CURL_SSLVERSION_TLSv1_1:
    case CURL_SSLVERSION_TLSv1_2:
    case CURL_SSLVERSION_TLSv1_3:
      {
        CURLcode result = set_ssl_version_min_max(&prioritylist, conn);
        if(result != CURLE_OK)
          return result;
        break;
      }
    case CURL_SSLVERSION_SSLv2:
      failf(data, "GnuTLS does not support SSLv2");
      return CURLE_SSL_CONNECT_ERROR;
    default:
      failf(data, "Unrecognized parameter passed via CURLOPT_SSLVERSION");
      return CURLE_SSL_CONNECT_ERROR;
  }
  rc = gnutls_priority_set_direct(session, prioritylist, &err);
  if((rc == GNUTLS_E_INVALID_REQUEST) && err) {
    if(!strcmp(err, GNUTLS_SRP)) {
      /* This GnuTLS was probably compiled without support for SRP.
       * Note that fact and try again without it. */
      int validprioritylen = curlx_uztosi(err - prioritylist);
      char *prioritycopy = strdup(prioritylist);
      if(!prioritycopy)
        return CURLE_OUT_OF_MEMORY;

      infof(data, "This GnuTLS does not support SRP\n");
      if(validprioritylen)
        /* Remove the :+SRP */
        prioritycopy[validprioritylen - 1] = 0;
      rc = gnutls_priority_set_direct(session, prioritycopy, &err);
      free(prioritycopy);
    }
  }
  if(rc != GNUTLS_E_SUCCESS) {
    failf(data, "Error %d setting GnuTLS cipher list starting with %s",
          rc, err);
    return CURLE_SSL_CONNECT_ERROR;
  }
#endif

#ifdef HAS_ALPN
  if(conn->bits.tls_enable_alpn) {
    int cur = 0;
    gnutls_datum_t protocols[2];

#ifdef USE_NGHTTP2
    if(data->set.httpversion >= CURL_HTTP_VERSION_2 &&
       (!SSL_IS_PROXY() || !conn->bits.tunnel_proxy)) {
      protocols[cur].data = (unsigned char *)NGHTTP2_PROTO_VERSION_ID;
      protocols[cur].size = NGHTTP2_PROTO_VERSION_ID_LEN;
      cur++;
      infof(data, "ALPN, offering %s\n", NGHTTP2_PROTO_VERSION_ID);
    }
#endif

    protocols[cur].data = (unsigned char *)ALPN_HTTP_1_1;
    protocols[cur].size = ALPN_HTTP_1_1_LENGTH;
    cur++;
    infof(data, "ALPN, offering %s\n", ALPN_HTTP_1_1);

    gnutls_alpn_set_protocols(session, protocols, cur, 0);
  }
#endif

  if(SSL_SET_OPTION(cert)) {
    if(SSL_SET_OPTION(key_passwd)) {
#if HAVE_GNUTLS_CERTIFICATE_SET_X509_KEY_FILE2
      const unsigned int supported_key_encryption_algorithms =
        GNUTLS_PKCS_USE_PKCS12_3DES | GNUTLS_PKCS_USE_PKCS12_ARCFOUR |
        GNUTLS_PKCS_USE_PKCS12_RC2_40 | GNUTLS_PKCS_USE_PBES2_3DES |
        GNUTLS_PKCS_USE_PBES2_AES_128 | GNUTLS_PKCS_USE_PBES2_AES_192 |
        GNUTLS_PKCS_USE_PBES2_AES_256;
      rc = gnutls_certificate_set_x509_key_file2(
           BACKEND->cred,
           SSL_SET_OPTION(cert),
           SSL_SET_OPTION(key) ?
           SSL_SET_OPTION(key) : SSL_SET_OPTION(cert),
           do_file_type(SSL_SET_OPTION(cert_type)),
           SSL_SET_OPTION(key_passwd),
           supported_key_encryption_algorithms);
      if(rc != GNUTLS_E_SUCCESS) {
        failf(data,
              "error reading X.509 potentially-encrypted key file: %s",
              gnutls_strerror(rc));
        return CURLE_SSL_CONNECT_ERROR;
      }
#else
      failf(data, "gnutls lacks support for encrypted key files");
      return CURLE_SSL_CONNECT_ERROR;
#endif
    }
    else {
      if(gnutls_certificate_set_x509_key_file(
           BACKEND->cred,
           SSL_SET_OPTION(cert),
           SSL_SET_OPTION(key) ?
           SSL_SET_OPTION(key) : SSL_SET_OPTION(cert),
           do_file_type(SSL_SET_OPTION(cert_type)) ) !=
         GNUTLS_E_SUCCESS) {
        failf(data, "error reading X.509 key or certificate file");
        return CURLE_SSL_CONNECT_ERROR;
      }
    }
  }

#ifdef USE_TLS_SRP
  /* put the credentials to the current session */
  if(SSL_SET_OPTION(authtype) == CURL_TLSAUTH_SRP) {
    rc = gnutls_credentials_set(session, GNUTLS_CRD_SRP,
                                BACKEND->srp_client_cred);
    if(rc != GNUTLS_E_SUCCESS) {
      failf(data, "gnutls_credentials_set() failed: %s", gnutls_strerror(rc));
      return CURLE_SSL_CONNECT_ERROR;
    }
  }
  else
#endif
  {
    rc = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
                                BACKEND->cred);
    if(rc != GNUTLS_E_SUCCESS) {
      failf(data, "gnutls_credentials_set() failed: %s", gnutls_strerror(rc));
      return CURLE_SSL_CONNECT_ERROR;
    }
  }

  if(conn->proxy_ssl[sockindex].use) {
    transport_ptr = conn->proxy_ssl[sockindex].backend->session;
    gnutls_transport_push = Curl_gtls_push_ssl;
    gnutls_transport_pull = Curl_gtls_pull_ssl;
  }
  else {
    /* file descriptor for the socket */
    transport_ptr = GNUTLS_SOCKET_TO_POINTER_CAST(conn->sock[sockindex]);
    gnutls_transport_push = Curl_gtls_push;
    gnutls_transport_pull = Curl_gtls_pull;
  }

  /* set the connection handle */
  gnutls_transport_set_ptr(session, transport_ptr);

  /* register callback functions to send and receive data. */
  gnutls_transport_set_push_function(session, gnutls_transport_push);
  gnutls_transport_set_pull_function(session, gnutls_transport_pull);

  /* lowat must be set to zero when using custom push and pull functions. */
  gnutls_transport_set_lowat(session, 0);

#ifdef HAS_OCSP
  if(SSL_CONN_CONFIG(verifystatus)) {
    rc = gnutls_ocsp_status_request_enable_client(session, NULL, 0, NULL);
    if(rc != GNUTLS_E_SUCCESS) {
      failf(data, "gnutls_ocsp_status_request_enable_client() failed: %d", rc);
      return CURLE_SSL_CONNECT_ERROR;
    }
  }
#endif

  /* This might be a reconnect, so we check for a session ID in the cache
     to speed up things */
  if(SSL_SET_OPTION(primary.sessionid)) {
    void *ssl_sessionid;
    size_t ssl_idsize;

    Curl_ssl_sessionid_lock(conn);
    if(!Curl_ssl_getsessionid(conn, &ssl_sessionid, &ssl_idsize, sockindex)) {
      /* we got a session id, use it! */
      gnutls_session_set_data(session, ssl_sessionid, ssl_idsize);

      /* Informational message */
      infof(data, "SSL re-using session ID\n");
    }
    Curl_ssl_sessionid_unlock(conn);
  }

  return CURLE_OK;
}

static CURLcode pkp_pin_peer_pubkey(struct Curl_easy *data,
                                    gnutls_x509_crt_t cert,
                                    const char *pinnedpubkey)
{
  /* Scratch */
  size_t len1 = 0, len2 = 0;
  unsigned char *buff1 = NULL;

  gnutls_pubkey_t key = NULL;

  /* Result is returned to caller */
  int ret = 0;
  CURLcode result = CURLE_SSL_PINNEDPUBKEYNOTMATCH;

  /* if a path wasn't specified, don't pin */
  if(NULL == pinnedpubkey)
    return CURLE_OK;

  if(NULL == cert)
    return result;

  do {
    /* Begin Gyrations to get the public key     */
    gnutls_pubkey_init(&key);

    ret = gnutls_pubkey_import_x509(key, cert, 0);
    if(ret < 0)
      break; /* failed */

    ret = gnutls_pubkey_export(key, GNUTLS_X509_FMT_DER, NULL, &len1);
    if(ret != GNUTLS_E_SHORT_MEMORY_BUFFER || len1 == 0)
      break; /* failed */

    buff1 = malloc(len1);
    if(NULL == buff1)
      break; /* failed */

    len2 = len1;

    ret = gnutls_pubkey_export(key, GNUTLS_X509_FMT_DER, buff1, &len2);
    if(ret < 0 || len1 != len2)
      break; /* failed */

    /* End Gyrations */

    /* The one good exit point */
    result = Curl_pin_peer_pubkey(data, pinnedpubkey, buff1, len1);
  } while(0);

  if(NULL != key)
    gnutls_pubkey_deinit(key);

  Curl_safefree(buff1);

  return result;
}

static Curl_recv gtls_recv;
static Curl_send gtls_send;

static CURLcode
gtls_connect_step3(struct connectdata *conn,
                   int sockindex)
{
  unsigned int cert_list_size;
  const gnutls_datum_t *chainp;
  unsigned int verify_status = 0;
  gnutls_x509_crt_t x509_cert, x509_issuer;
  gnutls_datum_t issuerp;
  char certbuf[256] = ""; /* big enough? */
  size_t size;
  time_t certclock;
  const char *ptr;
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  gnutls_session_t session = BACKEND->session;
  int rc;
#ifdef HAS_ALPN
  gnutls_datum_t proto;
#endif
  CURLcode result = CURLE_OK;
#ifndef CURL_DISABLE_VERBOSE_STRINGS
  unsigned int algo;
  unsigned int bits;
  gnutls_protocol_t version = gnutls_protocol_get_version(session);
#endif
  const char * const hostname = SSL_IS_PROXY() ? conn->http_proxy.host.name :
    conn->host.name;

  /* the name of the cipher suite used, e.g. ECDHE_RSA_AES_256_GCM_SHA384. */
  ptr = gnutls_cipher_suite_get_name(gnutls_kx_get(session),
                                     gnutls_cipher_get(session),
                                     gnutls_mac_get(session));

  infof(data, "SSL connection using %s / %s\n",
        gnutls_protocol_get_name(version), ptr);

  /* This function will return the peer's raw certificate (chain) as sent by
     the peer. These certificates are in raw format (DER encoded for
     X.509). In case of a X.509 then a certificate list may be present. The
     first certificate in the list is the peer's certificate, following the
     issuer's certificate, then the issuer's issuer etc. */

  chainp = gnutls_certificate_get_peers(session, &cert_list_size);
  if(!chainp) {
    if(SSL_CONN_CONFIG(verifypeer) ||
       SSL_CONN_CONFIG(verifyhost) ||
       SSL_SET_OPTION(issuercert)) {
#ifdef USE_TLS_SRP
      if(SSL_SET_OPTION(authtype) == CURL_TLSAUTH_SRP
         && SSL_SET_OPTION(username) != NULL
         && !SSL_CONN_CONFIG(verifypeer)
         && gnutls_cipher_get(session)) {
        /* no peer cert, but auth is ok if we have SRP user and cipher and no
           peer verify */
      }
      else {
#endif
        failf(data, "failed to get server cert");
        return CURLE_PEER_FAILED_VERIFICATION;
#ifdef USE_TLS_SRP
      }
#endif
    }
    infof(data, "\t common name: WARNING couldn't obtain\n");
  }

  if(data->set.ssl.certinfo && chainp) {
    unsigned int i;

    result = Curl_ssl_init_certinfo(data, cert_list_size);
    if(result)
      return result;

    for(i = 0; i < cert_list_size; i++) {
      const char *beg = (const char *) chainp[i].data;
      const char *end = beg + chainp[i].size;

      result = Curl_extract_certinfo(conn, i, beg, end);
      if(result)
        return result;
    }
  }

  if(SSL_CONN_CONFIG(verifypeer)) {
    /* This function will try to verify the peer's certificate and return its
       status (trusted, invalid etc.). The value of status should be one or
       more of the gnutls_certificate_status_t enumerated elements bitwise
       or'd. To avoid denial of service attacks some default upper limits
       regarding the certificate key size and chain size are set. To override
       them use gnutls_certificate_set_verify_limits(). */

    rc = gnutls_certificate_verify_peers2(session, &verify_status);
    if(rc < 0) {
      failf(data, "server cert verify failed: %d", rc);
      return CURLE_SSL_CONNECT_ERROR;
    }

    /* verify_status is a bitmask of gnutls_certificate_status bits */
    if(verify_status & GNUTLS_CERT_INVALID) {
      if(SSL_CONN_CONFIG(verifypeer)) {
        failf(data, "server certificate verification failed. CAfile: %s "
              "CRLfile: %s", SSL_CONN_CONFIG(CAfile) ? SSL_CONN_CONFIG(CAfile):
              "none",
              SSL_SET_OPTION(CRLfile)?SSL_SET_OPTION(CRLfile):"none");
        return CURLE_SSL_CACERT;
      }
      else
        infof(data, "\t server certificate verification FAILED\n");
    }
    else
      infof(data, "\t server certificate verification OK\n");
  }
  else
    infof(data, "\t server certificate verification SKIPPED\n");

#ifdef HAS_OCSP
  if(SSL_CONN_CONFIG(verifystatus)) {
    if(gnutls_ocsp_status_request_is_checked(session, 0) == 0) {
      gnutls_datum_t status_request;
      gnutls_ocsp_resp_t ocsp_resp;

      gnutls_ocsp_cert_status_t status;
      gnutls_x509_crl_reason_t reason;

      rc = gnutls_ocsp_status_request_get(session, &status_request);

      infof(data, "\t server certificate status verification FAILED\n");

      if(rc == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
        failf(data, "No OCSP response received");
        return CURLE_SSL_INVALIDCERTSTATUS;
      }

      if(rc < 0) {
        failf(data, "Invalid OCSP response received");
        return CURLE_SSL_INVALIDCERTSTATUS;
      }

      gnutls_ocsp_resp_init(&ocsp_resp);

      rc = gnutls_ocsp_resp_import(ocsp_resp, &status_request);
      if(rc < 0) {
        failf(data, "Invalid OCSP response received");
        return CURLE_SSL_INVALIDCERTSTATUS;
      }

      rc = gnutls_ocsp_resp_get_single(ocsp_resp, 0, NULL, NULL, NULL, NULL,
                                       &status, NULL, NULL, NULL, &reason);

      switch(status) {
      case GNUTLS_OCSP_CERT_GOOD:
        break;

      case GNUTLS_OCSP_CERT_REVOKED: {
        const char *crl_reason;

        switch(reason) {
          default:
          case GNUTLS_X509_CRLREASON_UNSPECIFIED:
            crl_reason = "unspecified reason";
            break;

          case GNUTLS_X509_CRLREASON_KEYCOMPROMISE:
            crl_reason = "private key compromised";
            break;

          case GNUTLS_X509_CRLREASON_CACOMPROMISE:
            crl_reason = "CA compromised";
            break;

          case GNUTLS_X509_CRLREASON_AFFILIATIONCHANGED:
            crl_reason = "affiliation has changed";
            break;

          case GNUTLS_X509_CRLREASON_SUPERSEDED:
            crl_reason = "certificate superseded";
            break;

          case GNUTLS_X509_CRLREASON_CESSATIONOFOPERATION:
            crl_reason = "operation has ceased";
            break;

          case GNUTLS_X509_CRLREASON_CERTIFICATEHOLD:
            crl_reason = "certificate is on hold";
            break;

          case GNUTLS_X509_CRLREASON_REMOVEFROMCRL:
            crl_reason = "will be removed from delta CRL";
            break;

          case GNUTLS_X509_CRLREASON_PRIVILEGEWITHDRAWN:
            crl_reason = "privilege withdrawn";
            break;

          case GNUTLS_X509_CRLREASON_AACOMPROMISE:
            crl_reason = "AA compromised";
            break;
        }

        failf(data, "Server certificate was revoked: %s", crl_reason);
        break;
      }

      default:
      case GNUTLS_OCSP_CERT_UNKNOWN:
        failf(data, "Server certificate status is unknown");
        break;
      }

      gnutls_ocsp_resp_deinit(ocsp_resp);

      return CURLE_SSL_INVALIDCERTSTATUS;
    }
    else
      infof(data, "\t server certificate status verification OK\n");
  }
  else
    infof(data, "\t server certificate status verification SKIPPED\n");
#endif

  /* initialize an X.509 certificate structure. */
  gnutls_x509_crt_init(&x509_cert);

  if(chainp)
    /* convert the given DER or PEM encoded Certificate to the native
       gnutls_x509_crt_t format */
    gnutls_x509_crt_import(x509_cert, chainp, GNUTLS_X509_FMT_DER);

  if(SSL_SET_OPTION(issuercert)) {
    gnutls_x509_crt_init(&x509_issuer);
    issuerp = load_file(SSL_SET_OPTION(issuercert));
    gnutls_x509_crt_import(x509_issuer, &issuerp, GNUTLS_X509_FMT_PEM);
    rc = gnutls_x509_crt_check_issuer(x509_cert, x509_issuer);
    gnutls_x509_crt_deinit(x509_issuer);
    unload_file(issuerp);
    if(rc <= 0) {
      failf(data, "server certificate issuer check failed (IssuerCert: %s)",
            SSL_SET_OPTION(issuercert)?SSL_SET_OPTION(issuercert):"none");
      gnutls_x509_crt_deinit(x509_cert);
      return CURLE_SSL_ISSUER_ERROR;
    }
    infof(data, "\t server certificate issuer check OK (Issuer Cert: %s)\n",
          SSL_SET_OPTION(issuercert)?SSL_SET_OPTION(issuercert):"none");
  }

  size = sizeof(certbuf);
  rc = gnutls_x509_crt_get_dn_by_oid(x509_cert, GNUTLS_OID_X520_COMMON_NAME,
                                     0, /* the first and only one */
                                     FALSE,
                                     certbuf,
                                     &size);
  if(rc) {
    infof(data, "error fetching CN from cert:%s\n",
          gnutls_strerror(rc));
  }

  /* This function will check if the given certificate's subject matches the
     given hostname. This is a basic implementation of the matching described
     in RFC2818 (HTTPS), which takes into account wildcards, and the subject
     alternative name PKIX extension. Returns non zero on success, and zero on
     failure. */
  rc = gnutls_x509_crt_check_hostname(x509_cert, hostname);
#if GNUTLS_VERSION_NUMBER < 0x030306
  /* Before 3.3.6, gnutls_x509_crt_check_hostname() didn't check IP
     addresses. */
  if(!rc) {
#ifdef ENABLE_IPV6
    #define use_addr in6_addr
#else
    #define use_addr in_addr
#endif
    unsigned char addrbuf[sizeof(struct use_addr)];
    unsigned char certaddr[sizeof(struct use_addr)];
    size_t addrlen = 0, certaddrlen;
    int i;
    int ret = 0;

    if(Curl_inet_pton(AF_INET, hostname, addrbuf) > 0)
      addrlen = 4;
#ifdef ENABLE_IPV6
    else if(Curl_inet_pton(AF_INET6, hostname, addrbuf) > 0)
      addrlen = 16;
#endif

    if(addrlen) {
      for(i = 0; ; i++) {
        certaddrlen = sizeof(certaddr);
        ret = gnutls_x509_crt_get_subject_alt_name(x509_cert, i, certaddr,
                                                   &certaddrlen, NULL);
        /* If this happens, it wasn't an IP address. */
        if(ret == GNUTLS_E_SHORT_MEMORY_BUFFER)
          continue;
        if(ret < 0)
          break;
        if(ret != GNUTLS_SAN_IPADDRESS)
          continue;
        if(certaddrlen == addrlen && !memcmp(addrbuf, certaddr, addrlen)) {
          rc = 1;
          break;
        }
      }
    }
  }
#endif
  if(!rc) {
    const char * const dispname = SSL_IS_PROXY() ?
      conn->http_proxy.host.dispname : conn->host.dispname;

    if(SSL_CONN_CONFIG(verifyhost)) {
      failf(data, "SSL: certificate subject name (%s) does not match "
            "target host name '%s'", certbuf, dispname);
      gnutls_x509_crt_deinit(x509_cert);
      return CURLE_PEER_FAILED_VERIFICATION;
    }
    else
      infof(data, "\t common name: %s (does not match '%s')\n",
            certbuf, dispname);
  }
  else
    infof(data, "\t common name: %s (matched)\n", certbuf);

  /* Check for time-based validity */
  certclock = gnutls_x509_crt_get_expiration_time(x509_cert);

  if(certclock == (time_t)-1) {
    if(SSL_CONN_CONFIG(verifypeer)) {
      failf(data, "server cert expiration date verify failed");
      gnutls_x509_crt_deinit(x509_cert);
      return CURLE_SSL_CONNECT_ERROR;
    }
    else
      infof(data, "\t server certificate expiration date verify FAILED\n");
  }
  else {
    if(certclock < time(NULL)) {
      if(SSL_CONN_CONFIG(verifypeer)) {
        failf(data, "server certificate expiration date has passed.");
        gnutls_x509_crt_deinit(x509_cert);
        return CURLE_PEER_FAILED_VERIFICATION;
      }
      else
        infof(data, "\t server certificate expiration date FAILED\n");
    }
    else
      infof(data, "\t server certificate expiration date OK\n");
  }

  certclock = gnutls_x509_crt_get_activation_time(x509_cert);

  if(certclock == (time_t)-1) {
    if(SSL_CONN_CONFIG(verifypeer)) {
      failf(data, "server cert activation date verify failed");
      gnutls_x509_crt_deinit(x509_cert);
      return CURLE_SSL_CONNECT_ERROR;
    }
    else
      infof(data, "\t server certificate activation date verify FAILED\n");
  }
  else {
    if(certclock > time(NULL)) {
      if(SSL_CONN_CONFIG(verifypeer)) {
        failf(data, "server certificate not activated yet.");
        gnutls_x509_crt_deinit(x509_cert);
        return CURLE_PEER_FAILED_VERIFICATION;
      }
      else
        infof(data, "\t server certificate activation date FAILED\n");
    }
    else
      infof(data, "\t server certificate activation date OK\n");
  }

  ptr = SSL_IS_PROXY() ? data->set.str[STRING_SSL_PINNEDPUBLICKEY_PROXY] :
        data->set.str[STRING_SSL_PINNEDPUBLICKEY_ORIG];
  if(ptr) {
    result = pkp_pin_peer_pubkey(data, x509_cert, ptr);
    if(result != CURLE_OK) {
      failf(data, "SSL: public key does not match pinned public key!");
      gnutls_x509_crt_deinit(x509_cert);
      return result;
    }
  }

  /* Show:

  - subject
  - start date
  - expire date
  - common name
  - issuer

  */

#ifndef CURL_DISABLE_VERBOSE_STRINGS
  /* public key algorithm's parameters */
  algo = gnutls_x509_crt_get_pk_algorithm(x509_cert, &bits);
  infof(data, "\t certificate public key: %s\n",
        gnutls_pk_algorithm_get_name(algo));

  /* version of the X.509 certificate. */
  infof(data, "\t certificate version: #%d\n",
        gnutls_x509_crt_get_version(x509_cert));


  size = sizeof(certbuf);
  gnutls_x509_crt_get_dn(x509_cert, certbuf, &size);
  infof(data, "\t subject: %s\n", certbuf);

  certclock = gnutls_x509_crt_get_activation_time(x509_cert);
  showtime(data, "start date", certclock);

  certclock = gnutls_x509_crt_get_expiration_time(x509_cert);
  showtime(data, "expire date", certclock);

  size = sizeof(certbuf);
  gnutls_x509_crt_get_issuer_dn(x509_cert, certbuf, &size);
  infof(data, "\t issuer: %s\n", certbuf);

  /* compression algorithm (if any) */
  ptr = gnutls_compression_get_name(gnutls_compression_get(session));
  /* the *_get_name() says "NULL" if GNUTLS_COMP_NULL is returned */
  infof(data, "\t compression: %s\n", ptr);
#endif

  gnutls_x509_crt_deinit(x509_cert);

#ifdef HAS_ALPN
  if(conn->bits.tls_enable_alpn) {
    rc = gnutls_alpn_get_selected_protocol(session, &proto);
    if(rc == 0) {
      infof(data, "ALPN, server accepted to use %.*s\n", proto.size,
          proto.data);

#ifdef USE_NGHTTP2
      if(proto.size == NGHTTP2_PROTO_VERSION_ID_LEN &&
         !memcmp(NGHTTP2_PROTO_VERSION_ID, proto.data,
                 NGHTTP2_PROTO_VERSION_ID_LEN)) {
        conn->negnpn = CURL_HTTP_VERSION_2;
      }
      else
#endif
      if(proto.size == ALPN_HTTP_1_1_LENGTH &&
         !memcmp(ALPN_HTTP_1_1, proto.data, ALPN_HTTP_1_1_LENGTH)) {
        conn->negnpn = CURL_HTTP_VERSION_1_1;
      }
    }
    else
      infof(data, "ALPN, server did not agree to a protocol\n");
  }
#endif

  conn->ssl[sockindex].state = ssl_connection_complete;
  conn->recv[sockindex] = gtls_recv;
  conn->send[sockindex] = gtls_send;

  if(SSL_SET_OPTION(primary.sessionid)) {
    /* we always unconditionally get the session id here, as even if we
       already got it from the cache and asked to use it in the connection, it
       might've been rejected and then a new one is in use now and we need to
       detect that. */
    bool incache;
    void *ssl_sessionid;
    void *connect_sessionid;
    size_t connect_idsize = 0;

    /* get the session ID data size */
    gnutls_session_get_data(session, NULL, &connect_idsize);
    connect_sessionid = malloc(connect_idsize); /* get a buffer for it */

    if(connect_sessionid) {
      /* extract session ID to the allocated buffer */
      gnutls_session_get_data(session, connect_sessionid, &connect_idsize);

      Curl_ssl_sessionid_lock(conn);
      incache = !(Curl_ssl_getsessionid(conn, &ssl_sessionid, NULL,
                                        sockindex));
      if(incache) {
        /* there was one before in the cache, so instead of risking that the
           previous one was rejected, we just kill that and store the new */
        Curl_ssl_delsessionid(conn, ssl_sessionid);
      }

      /* store this session id */
      result = Curl_ssl_addsessionid(conn, connect_sessionid, connect_idsize,
                                     sockindex);
      Curl_ssl_sessionid_unlock(conn);
      if(result) {
        free(connect_sessionid);
        result = CURLE_OUT_OF_MEMORY;
      }
    }
    else
      result = CURLE_OUT_OF_MEMORY;
  }

  return result;
}


/*
 * This function is called after the TCP connect has completed. Setup the TLS
 * layer and do all necessary magic.
 */
/* We use connssl->connecting_state to keep track of the connection status;
   there are three states: 'ssl_connect_1' (not started yet or complete),
   'ssl_connect_2_reading' (waiting for data from server), and
   'ssl_connect_2_writing' (waiting to be able to write).
 */
static CURLcode
gtls_connect_common(struct connectdata *conn,
                    int sockindex,
                    bool nonblocking,
                    bool *done)
{
  int rc;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];

  /* Initiate the connection, if not already done */
  if(ssl_connect_1 == connssl->connecting_state) {
    rc = gtls_connect_step1(conn, sockindex);
    if(rc)
      return rc;
  }

  rc = handshake(conn, sockindex, TRUE, nonblocking);
  if(rc)
    /* handshake() sets its own error message with failf() */
    return rc;

  /* Finish connecting once the handshake is done */
  if(ssl_connect_1 == connssl->connecting_state) {
    rc = gtls_connect_step3(conn, sockindex);
    if(rc)
      return rc;
  }

  *done = ssl_connect_1 == connssl->connecting_state;

  return CURLE_OK;
}

static CURLcode Curl_gtls_connect_nonblocking(struct connectdata *conn,
                                              int sockindex, bool *done)
{
  return gtls_connect_common(conn, sockindex, TRUE, done);
}

static CURLcode Curl_gtls_connect(struct connectdata *conn, int sockindex)
{
  CURLcode result;
  bool done = FALSE;

  result = gtls_connect_common(conn, sockindex, FALSE, &done);
  if(result)
    return result;

  DEBUGASSERT(done);

  return CURLE_OK;
}

static bool Curl_gtls_data_pending(const struct connectdata *conn,
                                   int connindex)
{
  const struct ssl_connect_data *connssl = &conn->ssl[connindex];
  bool res = FALSE;
  if(BACKEND->session &&
     0 != gnutls_record_check_pending(BACKEND->session))
    res = TRUE;

  connssl = &conn->proxy_ssl[connindex];
  if(BACKEND->session &&
     0 != gnutls_record_check_pending(BACKEND->session))
    res = TRUE;

  return res;
}

static ssize_t gtls_send(struct connectdata *conn,
                         int sockindex,
                         const void *mem,
                         size_t len,
                         CURLcode *curlcode)
{
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  ssize_t rc = gnutls_record_send(BACKEND->session, mem, len);

  if(rc < 0) {
    *curlcode = (rc == GNUTLS_E_AGAIN)
      ? CURLE_AGAIN
      : CURLE_SEND_ERROR;

    rc = -1;
  }

  return rc;
}

static void close_one(struct ssl_connect_data *connssl)
{
  if(BACKEND->session) {
    gnutls_bye(BACKEND->session, GNUTLS_SHUT_RDWR);
    gnutls_deinit(BACKEND->session);
    BACKEND->session = NULL;
  }
  if(BACKEND->cred) {
    gnutls_certificate_free_credentials(BACKEND->cred);
    BACKEND->cred = NULL;
  }
#ifdef USE_TLS_SRP
  if(BACKEND->srp_client_cred) {
    gnutls_srp_free_client_credentials(BACKEND->srp_client_cred);
    BACKEND->srp_client_cred = NULL;
  }
#endif
}

static void Curl_gtls_close(struct connectdata *conn, int sockindex)
{
  close_one(&conn->ssl[sockindex]);
  close_one(&conn->proxy_ssl[sockindex]);
}

/*
 * This function is called to shut down the SSL layer but keep the
 * socket open (CCC - Clear Command Channel)
 */
static int Curl_gtls_shutdown(struct connectdata *conn, int sockindex)
{
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  ssize_t result;
  int retval = 0;
  struct Curl_easy *data = conn->data;
  int done = 0;
  char buf[120];

  /* This has only been tested on the proftpd server, and the mod_tls code
     sends a close notify alert without waiting for a close notify alert in
     response. Thus we wait for a close notify alert from the server, but
     we do not send one. Let's hope other servers do the same... */

  if(data->set.ftp_ccc == CURLFTPSSL_CCC_ACTIVE)
      gnutls_bye(BACKEND->session, GNUTLS_SHUT_WR);

  if(BACKEND->session) {
    while(!done) {
      int what = SOCKET_READABLE(conn->sock[sockindex],
                                 SSL_SHUTDOWN_TIMEOUT);
      if(what > 0) {
        /* Something to read, let's do it and hope that it is the close
           notify alert from the server */
        result = gnutls_record_recv(BACKEND->session,
                                    buf, sizeof(buf));
        switch(result) {
        case 0:
          /* This is the expected response. There was no data but only
             the close notify alert */
          done = 1;
          break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
          infof(data, "GNUTLS_E_AGAIN || GNUTLS_E_INTERRUPTED\n");
          break;
        default:
          retval = -1;
          done = 1;
          break;
        }
      }
      else if(0 == what) {
        /* timeout */
        failf(data, "SSL shutdown timeout");
        done = 1;
        break;
      }
      else {
        /* anything that gets here is fatally bad */
        failf(data, "select/poll on SSL socket, errno: %d", SOCKERRNO);
        retval = -1;
        done = 1;
      }
    }
    gnutls_deinit(BACKEND->session);
  }
  gnutls_certificate_free_credentials(BACKEND->cred);

#ifdef USE_TLS_SRP
  if(SSL_SET_OPTION(authtype) == CURL_TLSAUTH_SRP
     && SSL_SET_OPTION(username) != NULL)
    gnutls_srp_free_client_credentials(BACKEND->srp_client_cred);
#endif

  BACKEND->cred = NULL;
  BACKEND->session = NULL;

  return retval;
}

static ssize_t gtls_recv(struct connectdata *conn, /* connection data */
                         int num,                  /* socketindex */
                         char *buf,                /* store read data here */
                         size_t buffersize,        /* max amount to read */
                         CURLcode *curlcode)
{
  struct ssl_connect_data *connssl = &conn->ssl[num];
  ssize_t ret;

  ret = gnutls_record_recv(BACKEND->session, buf, buffersize);
  if((ret == GNUTLS_E_AGAIN) || (ret == GNUTLS_E_INTERRUPTED)) {
    *curlcode = CURLE_AGAIN;
    return -1;
  }

  if(ret == GNUTLS_E_REHANDSHAKE) {
    /* BLOCKING call, this is bad but a work-around for now. Fixing this "the
       proper way" takes a whole lot of work. */
    CURLcode result = handshake(conn, num, FALSE, FALSE);
    if(result)
      /* handshake() writes error message on its own */
      *curlcode = result;
    else
      *curlcode = CURLE_AGAIN; /* then return as if this was a wouldblock */
    return -1;
  }

  if(ret < 0) {
    failf(conn->data, "GnuTLS recv error (%d): %s",

          (int)ret, gnutls_strerror((int)ret));
    *curlcode = CURLE_RECV_ERROR;
    return -1;
  }

  return ret;
}

static void Curl_gtls_session_free(void *ptr)
{
  free(ptr);
}

static size_t Curl_gtls_version(char *buffer, size_t size)
{
  return snprintf(buffer, size, "GnuTLS/%s", gnutls_check_version(NULL));
}

#ifndef USE_GNUTLS_NETTLE
static int Curl_gtls_seed(struct Curl_easy *data)
{
  /* we have the "SSL is seeded" boolean static to prevent multiple
     time-consuming seedings in vain */
  static bool ssl_seeded = FALSE;

  /* Quickly add a bit of entropy */
  gcry_fast_random_poll();

  if(!ssl_seeded || data->set.str[STRING_SSL_RANDOM_FILE] ||
     data->set.str[STRING_SSL_EGDSOCKET]) {

    /* TODO: to a good job seeding the RNG
       This may involve the gcry_control function and these options:
       GCRYCTL_SET_RANDOM_SEED_FILE
       GCRYCTL_SET_RNDEGD_SOCKET
    */
    ssl_seeded = TRUE;
  }
  return 0;
}
#endif

/* data might be NULL! */
static CURLcode Curl_gtls_random(struct Curl_easy *data,
                                 unsigned char *entropy, size_t length)
{
#if defined(USE_GNUTLS_NETTLE)
  int rc;
  (void)data;
  rc = gnutls_rnd(GNUTLS_RND_RANDOM, entropy, length);
  return rc?CURLE_FAILED_INIT:CURLE_OK;
#elif defined(USE_GNUTLS)
  if(data)
    Curl_gtls_seed(data); /* Initiate the seed if not already done */
  gcry_randomize(entropy, length, GCRY_STRONG_RANDOM);
#endif
  return CURLE_OK;
}

static CURLcode Curl_gtls_md5sum(unsigned char *tmp, /* input */
                                 size_t tmplen,
                                 unsigned char *md5sum, /* output */
                                 size_t md5len)
{
#if defined(USE_GNUTLS_NETTLE)
  struct md5_ctx MD5pw;
  md5_init(&MD5pw);
  md5_update(&MD5pw, (unsigned int)tmplen, tmp);
  md5_digest(&MD5pw, (unsigned int)md5len, md5sum);
#elif defined(USE_GNUTLS)
  gcry_md_hd_t MD5pw;
  gcry_md_open(&MD5pw, GCRY_MD_MD5, 0);
  gcry_md_write(MD5pw, tmp, tmplen);
  memcpy(md5sum, gcry_md_read(MD5pw, 0), md5len);
  gcry_md_close(MD5pw);
#endif
  return CURLE_OK;
}

static void Curl_gtls_sha256sum(const unsigned char *tmp, /* input */
                                size_t tmplen,
                                unsigned char *sha256sum, /* output */
                                size_t sha256len)
{
#if defined(USE_GNUTLS_NETTLE)
  struct sha256_ctx SHA256pw;
  sha256_init(&SHA256pw);
  sha256_update(&SHA256pw, (unsigned int)tmplen, tmp);
  sha256_digest(&SHA256pw, (unsigned int)sha256len, sha256sum);
#elif defined(USE_GNUTLS)
  gcry_md_hd_t SHA256pw;
  gcry_md_open(&SHA256pw, GCRY_MD_SHA256, 0);
  gcry_md_write(SHA256pw, tmp, tmplen);
  memcpy(sha256sum, gcry_md_read(SHA256pw, 0), sha256len);
  gcry_md_close(SHA256pw);
#endif
}

static bool Curl_gtls_cert_status_request(void)
{
#ifdef HAS_OCSP
  return TRUE;
#else
  return FALSE;
#endif
}

static void *Curl_gtls_get_internals(struct ssl_connect_data *connssl,
                                     CURLINFO info UNUSED_PARAM)
{
  (void)info;
  return BACKEND->session;
}

const struct Curl_ssl Curl_ssl_gnutls = {
  { CURLSSLBACKEND_GNUTLS, "gnutls" }, /* info */

  1, /* have_ca_path */
  1, /* have_certinfo */
  1, /* have_pinnedpubkey */
  0, /* have_ssl_ctx */
  1, /* support_https_proxy */

  sizeof(struct ssl_backend_data),

  Curl_gtls_init,                /* init */
  Curl_gtls_cleanup,             /* cleanup */
  Curl_gtls_version,             /* version */
  Curl_none_check_cxn,           /* check_cxn */
  Curl_gtls_shutdown,            /* shutdown */
  Curl_gtls_data_pending,        /* data_pending */
  Curl_gtls_random,              /* random */
  Curl_gtls_cert_status_request, /* cert_status_request */
  Curl_gtls_connect,             /* connect */
  Curl_gtls_connect_nonblocking, /* connect_nonblocking */
  Curl_gtls_get_internals,       /* get_internals */
  Curl_gtls_close,               /* close_one */
  Curl_none_close_all,           /* close_all */
  Curl_gtls_session_free,        /* session_free */
  Curl_none_set_engine,          /* set_engine */
  Curl_none_set_engine_default,  /* set_engine_default */
  Curl_none_engines_list,        /* engines_list */
  Curl_none_false_start,         /* false_start */
  Curl_gtls_md5sum,              /* md5sum */
  Curl_gtls_sha256sum            /* sha256sum */
};

#endif /* USE_GNUTLS */
