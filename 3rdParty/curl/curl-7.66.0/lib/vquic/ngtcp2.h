#ifndef HEADER_CURL_VQUIC_NGTCP2_H
#define HEADER_CURL_VQUIC_NGTCP2_H
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

#ifdef USE_NGTCP2

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>
#include <openssl/ssl.h>

struct quic_handshake {
  char *buf;       /* pointer to the buffer */
  size_t alloclen; /* size of allocation */
  size_t len;      /* size of content in buffer */
  size_t nread;    /* how many bytes have been read */
};

struct quicsocket {
  struct connectdata *conn; /* point back to the connection */
  ngtcp2_conn *qconn;
  ngtcp2_cid dcid;
  ngtcp2_cid scid;
  uint32_t version;
  ngtcp2_settings settings;
  SSL_CTX *sslctx;
  SSL *ssl;
  struct quic_handshake client_crypto_data[3];
  /* the last TLS alert description generated by the local endpoint */
  uint8_t tls_alert;
  struct sockaddr_storage local_addr;
  socklen_t local_addrlen;

  nghttp3_conn *h3conn;
  nghttp3_conn_settings h3settings;
};

#include "urldata.h"

#endif

#endif /* HEADER_CURL_VQUIC_NGTCP2_H */
