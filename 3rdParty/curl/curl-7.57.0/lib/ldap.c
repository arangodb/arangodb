/***************************************************************************
 *                      _   _ ____  _
 *  Project         ___| | | |  _ \| |
 *                 / __| | | | |_) | |
 *                | (__| |_| |  _ <| |___
 *                 \___|\___/|_| \_\_____|
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

#include "curl_setup.h"

#if !defined(CURL_DISABLE_LDAP) && !defined(USE_OPENLDAP)

/*
 * Notice that USE_OPENLDAP is only a source code selection switch. When
 * libcurl is built with USE_OPENLDAP defined the libcurl source code that
 * gets compiled is the code from openldap.c, otherwise the code that gets
 * compiled is the code from ldap.c.
 *
 * When USE_OPENLDAP is defined a recent version of the OpenLDAP library
 * might be required for compilation and runtime. In order to use ancient
 * OpenLDAP library versions, USE_OPENLDAP shall not be defined.
 */

#ifdef USE_WIN32_LDAP           /* Use Windows LDAP implementation. */
# include <winldap.h>
# ifndef LDAP_VENDOR_NAME
#  error Your Platform SDK is NOT sufficient for LDAP support! \
         Update your Platform SDK, or disable LDAP support!
# else
#  include <winber.h>
# endif
#else
# define LDAP_DEPRECATED 1      /* Be sure ldap_init() is defined. */
# ifdef HAVE_LBER_H
#  include <lber.h>
# endif
# include <ldap.h>
# if (defined(HAVE_LDAP_SSL) && defined(HAVE_LDAP_SSL_H))
#  include <ldap_ssl.h>
# endif /* HAVE_LDAP_SSL && HAVE_LDAP_SSL_H */
#endif

/* These are macros in both <wincrypt.h> (in above <winldap.h>) and typedefs
 * in BoringSSL's <openssl/x509.h>
 */
#ifdef HAVE_BORINGSSL
# undef X509_NAME
# undef X509_CERT_PAIR
# undef X509_EXTENSIONS
#endif

#include "urldata.h"
#include <curl/curl.h>
#include "sendf.h"
#include "escape.h"
#include "progress.h"
#include "transfer.h"
#include "strcase.h"
#include "strtok.h"
#include "curl_ldap.h"
#include "curl_multibyte.h"
#include "curl_base64.h"
#include "connect.h"
/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#ifndef HAVE_LDAP_URL_PARSE

/* Use our own implementation. */

typedef struct {
  char   *lud_host;
  int     lud_port;
#if defined(USE_WIN32_LDAP)
  TCHAR  *lud_dn;
  TCHAR **lud_attrs;
#else
  char   *lud_dn;
  char  **lud_attrs;
#endif
  int     lud_scope;
#if defined(USE_WIN32_LDAP)
  TCHAR  *lud_filter;
#else
  char   *lud_filter;
#endif
  char  **lud_exts;
  size_t    lud_attrs_dups; /* how many were dup'ed, this field is not in the
                               "real" struct so can only be used in code
                               without HAVE_LDAP_URL_PARSE defined */
} CURL_LDAPURLDesc;

#undef LDAPURLDesc
#define LDAPURLDesc             CURL_LDAPURLDesc

static int  _ldap_url_parse(const struct connectdata *conn,
                            LDAPURLDesc **ludp);
static void _ldap_free_urldesc(LDAPURLDesc *ludp);

#undef ldap_free_urldesc
#define ldap_free_urldesc       _ldap_free_urldesc
#endif

#ifdef DEBUG_LDAP
  #define LDAP_TRACE(x)   do { \
                            _ldap_trace("%u: ", __LINE__); \
                            _ldap_trace x; \
                          } WHILE_FALSE

  static void _ldap_trace(const char *fmt, ...);
#else
  #define LDAP_TRACE(x)   Curl_nop_stmt
#endif


static CURLcode Curl_ldap(struct connectdata *conn, bool *done);

/*
 * LDAP protocol handler.
 */

const struct Curl_handler Curl_handler_ldap = {
  "LDAP",                               /* scheme */
  ZERO_NULL,                            /* setup_connection */
  Curl_ldap,                            /* do_it */
  ZERO_NULL,                            /* done */
  ZERO_NULL,                            /* do_more */
  ZERO_NULL,                            /* connect_it */
  ZERO_NULL,                            /* connecting */
  ZERO_NULL,                            /* doing */
  ZERO_NULL,                            /* proto_getsock */
  ZERO_NULL,                            /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  ZERO_NULL,                            /* perform_getsock */
  ZERO_NULL,                            /* disconnect */
  ZERO_NULL,                            /* readwrite */
  ZERO_NULL,                            /* connection_check */
  PORT_LDAP,                            /* defport */
  CURLPROTO_LDAP,                       /* protocol */
  PROTOPT_NONE                          /* flags */
};

#ifdef HAVE_LDAP_SSL
/*
 * LDAPS protocol handler.
 */

const struct Curl_handler Curl_handler_ldaps = {
  "LDAPS",                              /* scheme */
  ZERO_NULL,                            /* setup_connection */
  Curl_ldap,                            /* do_it */
  ZERO_NULL,                            /* done */
  ZERO_NULL,                            /* do_more */
  ZERO_NULL,                            /* connect_it */
  ZERO_NULL,                            /* connecting */
  ZERO_NULL,                            /* doing */
  ZERO_NULL,                            /* proto_getsock */
  ZERO_NULL,                            /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  ZERO_NULL,                            /* perform_getsock */
  ZERO_NULL,                            /* disconnect */
  ZERO_NULL,                            /* readwrite */
  ZERO_NULL,                            /* connection_check */
  PORT_LDAPS,                           /* defport */
  CURLPROTO_LDAPS,                      /* protocol */
  PROTOPT_SSL                           /* flags */
};
#endif

#if defined(USE_WIN32_LDAP)

#if defined(USE_WINDOWS_SSPI)
static int ldap_win_bind_auth(LDAP *server, const char *user,
                              const char *passwd, unsigned long authflags)
{
  ULONG method = 0;
  SEC_WINNT_AUTH_IDENTITY cred;
  int rc = LDAP_AUTH_METHOD_NOT_SUPPORTED;

  memset(&cred, 0, sizeof(cred));

#if defined(USE_SPNEGO)
  if(authflags & CURLAUTH_NEGOTIATE) {
    method = LDAP_AUTH_NEGOTIATE;
  }
  else
#endif
#if defined(USE_NTLM)
  if(authflags & CURLAUTH_NTLM) {
    method = LDAP_AUTH_NTLM;
  }
  else
#endif
#if !defined(CURL_DISABLE_CRYPTO_AUTH)
  if(authflags & CURLAUTH_DIGEST) {
    method = LDAP_AUTH_DIGEST;
  }
  else
#endif
  {
    /* required anyway if one of upper preprocessor definitions enabled */
  }

  if(method && user && passwd) {
    rc = Curl_create_sspi_identity(user, passwd, &cred);
    if(!rc) {
      rc = ldap_bind_s(server, NULL, (TCHAR *)&cred, method);
      Curl_sspi_free_identity(&cred);
    }
  }
  else {
    /* proceed with current user credentials */
    method = LDAP_AUTH_NEGOTIATE;
    rc = ldap_bind_s(server, NULL, NULL, method);
  }
  return rc;
}
#endif /* #if defined(USE_WINDOWS_SSPI) */

static int ldap_win_bind(struct connectdata *conn, LDAP *server,
                         const char *user, const char *passwd)
{
  int rc = LDAP_INVALID_CREDENTIALS;

  PTCHAR inuser = NULL;
  PTCHAR inpass = NULL;

  if(user && passwd && (conn->data->set.httpauth & CURLAUTH_BASIC)) {
    inuser = Curl_convert_UTF8_to_tchar((char *) user);
    inpass = Curl_convert_UTF8_to_tchar((char *) passwd);

    rc = ldap_simple_bind_s(server, inuser, inpass);

    Curl_unicodefree(inuser);
    Curl_unicodefree(inpass);
  }
#if defined(USE_WINDOWS_SSPI)
  else {
    rc = ldap_win_bind_auth(server, user, passwd, conn->data->set.httpauth);
  }
#endif

  return rc;
}
#endif /* #if defined(USE_WIN32_LDAP) */

static CURLcode Curl_ldap(struct connectdata *conn, bool *done)
{
  CURLcode result = CURLE_OK;
  int rc = 0;
  LDAP *server = NULL;
  LDAPURLDesc *ludp = NULL;
  LDAPMessage *ldapmsg = NULL;
  LDAPMessage *entryIterator;
  int num = 0;
  struct Curl_easy *data = conn->data;
  int ldap_proto = LDAP_VERSION3;
  int ldap_ssl = 0;
  char *val_b64 = NULL;
  size_t val_b64_sz = 0;
  curl_off_t dlsize = 0;
#ifdef LDAP_OPT_NETWORK_TIMEOUT
  struct timeval ldap_timeout = {10, 0}; /* 10 sec connection/search timeout */
#endif
#if defined(USE_WIN32_LDAP)
  TCHAR *host = NULL;
#else
  char *host = NULL;
#endif
  char *user = NULL;
  char *passwd = NULL;

  *done = TRUE; /* unconditionally */
  infof(data, "LDAP local: LDAP Vendor = %s ; LDAP Version = %d\n",
          LDAP_VENDOR_NAME, LDAP_VENDOR_VERSION);
  infof(data, "LDAP local: %s\n", data->change.url);

#ifdef HAVE_LDAP_URL_PARSE
  rc = ldap_url_parse(data->change.url, &ludp);
#else
  rc = _ldap_url_parse(conn, &ludp);
#endif
  if(rc != 0) {
    failf(data, "LDAP local: %s", ldap_err2string(rc));
    result = CURLE_LDAP_INVALID_URL;
    goto quit;
  }

  /* Get the URL scheme (either ldap or ldaps) */
  if(conn->given->flags & PROTOPT_SSL)
    ldap_ssl = 1;
  infof(data, "LDAP local: trying to establish %s connection\n",
          ldap_ssl ? "encrypted" : "cleartext");

#if defined(USE_WIN32_LDAP)
  host = Curl_convert_UTF8_to_tchar(conn->host.name);
  if(!host) {
    result = CURLE_OUT_OF_MEMORY;

    goto quit;
  }
#else
  host = conn->host.name;
#endif

  if(conn->bits.user_passwd) {
    user = conn->user;
    passwd = conn->passwd;
  }

#ifdef LDAP_OPT_NETWORK_TIMEOUT
  ldap_set_option(NULL, LDAP_OPT_NETWORK_TIMEOUT, &ldap_timeout);
#endif
  ldap_set_option(NULL, LDAP_OPT_PROTOCOL_VERSION, &ldap_proto);

  if(ldap_ssl) {
#ifdef HAVE_LDAP_SSL
#ifdef USE_WIN32_LDAP
    /* Win32 LDAP SDK doesn't support insecure mode without CA! */
    server = ldap_sslinit(host, (int)conn->port, 1);
    ldap_set_option(server, LDAP_OPT_SSL, LDAP_OPT_ON);
#else
    int ldap_option;
    char *ldap_ca = conn->ssl_config.CAfile;
#if defined(CURL_HAS_NOVELL_LDAPSDK)
    rc = ldapssl_client_init(NULL, NULL);
    if(rc != LDAP_SUCCESS) {
      failf(data, "LDAP local: ldapssl_client_init %s", ldap_err2string(rc));
      result = CURLE_SSL_CERTPROBLEM;
      goto quit;
    }
    if(conn->ssl_config.verifypeer) {
      /* Novell SDK supports DER or BASE64 files. */
      int cert_type = LDAPSSL_CERT_FILETYPE_B64;
      if((data->set.ssl.cert_type) &&
         (strcasecompare(data->set.ssl.cert_type, "DER")))
        cert_type = LDAPSSL_CERT_FILETYPE_DER;
      if(!ldap_ca) {
        failf(data, "LDAP local: ERROR %s CA cert not set!",
              (cert_type == LDAPSSL_CERT_FILETYPE_DER ? "DER" : "PEM"));
        result = CURLE_SSL_CERTPROBLEM;
        goto quit;
      }
      infof(data, "LDAP local: using %s CA cert '%s'\n",
              (cert_type == LDAPSSL_CERT_FILETYPE_DER ? "DER" : "PEM"),
              ldap_ca);
      rc = ldapssl_add_trusted_cert(ldap_ca, cert_type);
      if(rc != LDAP_SUCCESS) {
        failf(data, "LDAP local: ERROR setting %s CA cert: %s",
                (cert_type == LDAPSSL_CERT_FILETYPE_DER ? "DER" : "PEM"),
                ldap_err2string(rc));
        result = CURLE_SSL_CERTPROBLEM;
        goto quit;
      }
      ldap_option = LDAPSSL_VERIFY_SERVER;
    }
    else
      ldap_option = LDAPSSL_VERIFY_NONE;
    rc = ldapssl_set_verify_mode(ldap_option);
    if(rc != LDAP_SUCCESS) {
      failf(data, "LDAP local: ERROR setting cert verify mode: %s",
              ldap_err2string(rc));
      result = CURLE_SSL_CERTPROBLEM;
      goto quit;
    }
    server = ldapssl_init(host, (int)conn->port, 1);
    if(server == NULL) {
      failf(data, "LDAP local: Cannot connect to %s:%ld",
            conn->host.dispname, conn->port);
      result = CURLE_COULDNT_CONNECT;
      goto quit;
    }
#elif defined(LDAP_OPT_X_TLS)
    if(conn->ssl_config.verifypeer) {
      /* OpenLDAP SDK supports BASE64 files. */
      if((data->set.ssl.cert_type) &&
         (!strcasecompare(data->set.ssl.cert_type, "PEM"))) {
        failf(data, "LDAP local: ERROR OpenLDAP only supports PEM cert-type!");
        result = CURLE_SSL_CERTPROBLEM;
        goto quit;
      }
      if(!ldap_ca) {
        failf(data, "LDAP local: ERROR PEM CA cert not set!");
        result = CURLE_SSL_CERTPROBLEM;
        goto quit;
      }
      infof(data, "LDAP local: using PEM CA cert: %s\n", ldap_ca);
      rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_CACERTFILE, ldap_ca);
      if(rc != LDAP_SUCCESS) {
        failf(data, "LDAP local: ERROR setting PEM CA cert: %s",
                ldap_err2string(rc));
        result = CURLE_SSL_CERTPROBLEM;
        goto quit;
      }
      ldap_option = LDAP_OPT_X_TLS_DEMAND;
    }
    else
      ldap_option = LDAP_OPT_X_TLS_NEVER;

    rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &ldap_option);
    if(rc != LDAP_SUCCESS) {
      failf(data, "LDAP local: ERROR setting cert verify mode: %s",
              ldap_err2string(rc));
      result = CURLE_SSL_CERTPROBLEM;
      goto quit;
    }
    server = ldap_init(host, (int)conn->port);
    if(server == NULL) {
      failf(data, "LDAP local: Cannot connect to %s:%ld",
            conn->host.dispname, conn->port);
      result = CURLE_COULDNT_CONNECT;
      goto quit;
    }
    ldap_option = LDAP_OPT_X_TLS_HARD;
    rc = ldap_set_option(server, LDAP_OPT_X_TLS, &ldap_option);
    if(rc != LDAP_SUCCESS) {
      failf(data, "LDAP local: ERROR setting SSL/TLS mode: %s",
              ldap_err2string(rc));
      result = CURLE_SSL_CERTPROBLEM;
      goto quit;
    }
/*
    rc = ldap_start_tls_s(server, NULL, NULL);
    if(rc != LDAP_SUCCESS) {
      failf(data, "LDAP local: ERROR starting SSL/TLS mode: %s",
              ldap_err2string(rc));
      result = CURLE_SSL_CERTPROBLEM;
      goto quit;
    }
*/
#else
    /* we should probably never come up to here since configure
       should check in first place if we can support LDAP SSL/TLS */
    failf(data, "LDAP local: SSL/TLS not supported with this version "
            "of the OpenLDAP toolkit\n");
    result = CURLE_SSL_CERTPROBLEM;
    goto quit;
#endif
#endif
#endif /* CURL_LDAP_USE_SSL */
  }
  else {
    server = ldap_init(host, (int)conn->port);
    if(server == NULL) {
      failf(data, "LDAP local: Cannot connect to %s:%ld",
            conn->host.dispname, conn->port);
      result = CURLE_COULDNT_CONNECT;
      goto quit;
    }
  }
#ifdef USE_WIN32_LDAP
  ldap_set_option(server, LDAP_OPT_PROTOCOL_VERSION, &ldap_proto);
#endif

#ifdef USE_WIN32_LDAP
  rc = ldap_win_bind(conn, server, user, passwd);
#else
  rc = ldap_simple_bind_s(server, user, passwd);
#endif
  if(!ldap_ssl && rc != 0) {
    ldap_proto = LDAP_VERSION2;
    ldap_set_option(server, LDAP_OPT_PROTOCOL_VERSION, &ldap_proto);
#ifdef USE_WIN32_LDAP
    rc = ldap_win_bind(conn, server, user, passwd);
#else
    rc = ldap_simple_bind_s(server, user, passwd);
#endif
  }
  if(rc != 0) {
    failf(data, "LDAP local: ldap_simple_bind_s %s", ldap_err2string(rc));
    result = CURLE_LDAP_CANNOT_BIND;
    goto quit;
  }

  rc = ldap_search_s(server, ludp->lud_dn, ludp->lud_scope,
                     ludp->lud_filter, ludp->lud_attrs, 0, &ldapmsg);

  if(rc != 0 && rc != LDAP_SIZELIMIT_EXCEEDED) {
    failf(data, "LDAP remote: %s", ldap_err2string(rc));
    result = CURLE_LDAP_SEARCH_FAILED;
    goto quit;
  }

  for(num = 0, entryIterator = ldap_first_entry(server, ldapmsg);
      entryIterator;
      entryIterator = ldap_next_entry(server, entryIterator), num++) {
    BerElement *ber = NULL;
#if defined(USE_WIN32_LDAP)
    TCHAR *attribute;
#else
    char  *attribute;       /*! suspicious that this isn't 'const' */
#endif
    int i;

    /* Get the DN and write it to the client */
    {
      char *name;
      size_t name_len;
#if defined(USE_WIN32_LDAP)
      TCHAR *dn = ldap_get_dn(server, entryIterator);
      name = Curl_convert_tchar_to_UTF8(dn);
      if(!name) {
        ldap_memfree(dn);

        result = CURLE_OUT_OF_MEMORY;

        goto quit;
      }
#else
      char *dn = name = ldap_get_dn(server, entryIterator);
#endif
      name_len = strlen(name);

      result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)"DN: ", 4);
      if(result) {
#if defined(USE_WIN32_LDAP)
        Curl_unicodefree(name);
#endif
        ldap_memfree(dn);

        goto quit;
      }

      result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *) name,
                                 name_len);
      if(result) {
#if defined(USE_WIN32_LDAP)
        Curl_unicodefree(name);
#endif
        ldap_memfree(dn);

        goto quit;
      }

      result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)"\n", 1);
      if(result) {
#if defined(USE_WIN32_LDAP)
        Curl_unicodefree(name);
#endif
        ldap_memfree(dn);

        goto quit;
      }

      dlsize += name_len + 5;

#if defined(USE_WIN32_LDAP)
      Curl_unicodefree(name);
#endif
      ldap_memfree(dn);
    }

    /* Get the attributes and write them to the client */
    for(attribute = ldap_first_attribute(server, entryIterator, &ber);
        attribute;
        attribute = ldap_next_attribute(server, entryIterator, ber)) {
      BerValue **vals;
      size_t attr_len;
#if defined(USE_WIN32_LDAP)
      char *attr = Curl_convert_tchar_to_UTF8(attribute);
      if(!attr) {
        if(ber)
          ber_free(ber, 0);

        result = CURLE_OUT_OF_MEMORY;

        goto quit;
    }
#else
      char *attr = attribute;
#endif
      attr_len = strlen(attr);

      vals = ldap_get_values_len(server, entryIterator, attribute);
      if(vals != NULL) {
        for(i = 0; (vals[i] != NULL); i++) {
          result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)"\t", 1);
          if(result) {
            ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
            Curl_unicodefree(attr);
#endif
            ldap_memfree(attribute);
            if(ber)
              ber_free(ber, 0);

            goto quit;
          }

          result = Curl_client_write(conn, CLIENTWRITE_BODY,
                                     (char *) attr, attr_len);
          if(result) {
            ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
            Curl_unicodefree(attr);
#endif
            ldap_memfree(attribute);
            if(ber)
              ber_free(ber, 0);

            goto quit;
          }

          result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)": ", 2);
          if(result) {
            ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
            Curl_unicodefree(attr);
#endif
            ldap_memfree(attribute);
            if(ber)
              ber_free(ber, 0);

            goto quit;
          }

          dlsize += attr_len + 3;

          if((attr_len > 7) &&
             (strcmp(";binary", (char *) attr + (attr_len - 7)) == 0)) {
            /* Binary attribute, encode to base64. */
            result = Curl_base64_encode(data,
                                        vals[i]->bv_val,
                                        vals[i]->bv_len,
                                        &val_b64,
                                        &val_b64_sz);
            if(result) {
              ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
              Curl_unicodefree(attr);
#endif
              ldap_memfree(attribute);
              if(ber)
                ber_free(ber, 0);

              goto quit;
            }

            if(val_b64_sz > 0) {
              result = Curl_client_write(conn, CLIENTWRITE_BODY, val_b64,
                                         val_b64_sz);
              free(val_b64);
              if(result) {
                ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
                Curl_unicodefree(attr);
#endif
                ldap_memfree(attribute);
                if(ber)
                  ber_free(ber, 0);

                goto quit;
              }

              dlsize += val_b64_sz;
            }
          }
          else {
            result = Curl_client_write(conn, CLIENTWRITE_BODY, vals[i]->bv_val,
                                       vals[i]->bv_len);
            if(result) {
              ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
              Curl_unicodefree(attr);
#endif
              ldap_memfree(attribute);
              if(ber)
                ber_free(ber, 0);

              goto quit;
            }

            dlsize += vals[i]->bv_len;
          }

          result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)"\n", 1);
          if(result) {
            ldap_value_free_len(vals);
#if defined(USE_WIN32_LDAP)
            Curl_unicodefree(attr);
#endif
            ldap_memfree(attribute);
            if(ber)
              ber_free(ber, 0);

            goto quit;
          }

          dlsize++;
        }

        /* Free memory used to store values */
        ldap_value_free_len(vals);
      }

      /* Free the attribute as we are done with it */
#if defined(USE_WIN32_LDAP)
      Curl_unicodefree(attr);
#endif
      ldap_memfree(attribute);

      result = Curl_client_write(conn, CLIENTWRITE_BODY, (char *)"\n", 1);
      if(result)
        goto quit;
      dlsize++;
      Curl_pgrsSetDownloadCounter(data, dlsize);
    }

    if(ber)
       ber_free(ber, 0);
  }

quit:
  if(ldapmsg) {
    ldap_msgfree(ldapmsg);
    LDAP_TRACE(("Received %d entries\n", num));
  }
  if(rc == LDAP_SIZELIMIT_EXCEEDED)
    infof(data, "There are more than %d entries\n", num);
  if(ludp)
    ldap_free_urldesc(ludp);
  if(server)
    ldap_unbind_s(server);
#if defined(HAVE_LDAP_SSL) && defined(CURL_HAS_NOVELL_LDAPSDK)
  if(ldap_ssl)
    ldapssl_client_deinit();
#endif /* HAVE_LDAP_SSL && CURL_HAS_NOVELL_LDAPSDK */

#if defined(USE_WIN32_LDAP)
  Curl_unicodefree(host);
#endif

  /* no data to transfer */
  Curl_setup_transfer(conn, -1, -1, FALSE, NULL, -1, NULL);
  connclose(conn, "LDAP connection always disable re-use");

  return result;
}

#ifdef DEBUG_LDAP
static void _ldap_trace(const char *fmt, ...)
{
  static int do_trace = -1;
  va_list args;

  if(do_trace == -1) {
    const char *env = getenv("CURL_TRACE");
    do_trace = (env && strtol(env, NULL, 10) > 0);
  }
  if(!do_trace)
    return;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}
#endif

#ifndef HAVE_LDAP_URL_PARSE

/*
 * Return scope-value for a scope-string.
 */
static int str2scope(const char *p)
{
  if(strcasecompare(p, "one"))
    return LDAP_SCOPE_ONELEVEL;
  if(strcasecompare(p, "onetree"))
    return LDAP_SCOPE_ONELEVEL;
  if(strcasecompare(p, "base"))
    return LDAP_SCOPE_BASE;
  if(strcasecompare(p, "sub"))
    return LDAP_SCOPE_SUBTREE;
  if(strcasecompare(p, "subtree"))
    return LDAP_SCOPE_SUBTREE;
  return (-1);
}

/*
 * Split 'str' into strings separated by commas.
 * Note: out[] points into 'str'.
 */
static bool split_str(char *str, char ***out, size_t *count)
{
  char **res;
  char *lasts;
  char *s;
  size_t  i;
  size_t items = 1;

  s = strchr(str, ',');
  while(s) {
    items++;
    s = strchr(++s, ',');
  }

  res = calloc(items, sizeof(char *));
  if(!res)
    return FALSE;

  for(i = 0, s = strtok_r(str, ",", &lasts); s && i < items;
      s = strtok_r(NULL, ",", &lasts), i++)
    res[i] = s;

  *out = res;
  *count = items;

  return TRUE;
}

/*
 * Break apart the pieces of an LDAP URL.
 * Syntax:
 *   ldap://<hostname>:<port>/<base_dn>?<attributes>?<scope>?<filter>?<ext>
 *
 * <hostname> already known from 'conn->host.name'.
 * <port>     already known from 'conn->remote_port'.
 * extract the rest from 'conn->data->state.path+1'. All fields are optional.
 * e.g.
 *   ldap://<hostname>:<port>/?<attributes>?<scope>?<filter>
 * yields ludp->lud_dn = "".
 *
 * Defined in RFC4516 section 2.
 */
static int _ldap_url_parse2(const struct connectdata *conn, LDAPURLDesc *ludp)
{
  int rc = LDAP_SUCCESS;
  char *path;
  char *p;
  char *q;
  size_t i;

  if(!conn->data ||
     !conn->data->state.path ||
     conn->data->state.path[0] != '/' ||
     !checkprefix("LDAP", conn->data->change.url))
    return LDAP_INVALID_SYNTAX;

  ludp->lud_scope = LDAP_SCOPE_BASE;
  ludp->lud_port  = conn->remote_port;
  ludp->lud_host  = conn->host.name;

  /* Duplicate the path */
  p = path = strdup(conn->data->state.path + 1);
  if(!path)
    return LDAP_NO_MEMORY;

  /* Parse the DN (Distinguished Name) */
  q = strchr(p, '?');
  if(q)
    *q++ = '\0';

  if(*p) {
    char *dn = p;
    char *unescaped;
    CURLcode result;

    LDAP_TRACE(("DN '%s'\n", dn));

    /* Unescape the DN */
    result = Curl_urldecode(conn->data, dn, 0, &unescaped, NULL, FALSE);
    if(result) {
      rc = LDAP_NO_MEMORY;

      goto quit;
    }

#if defined(USE_WIN32_LDAP)
    /* Convert the unescaped string to a tchar */
    ludp->lud_dn = Curl_convert_UTF8_to_tchar(unescaped);

    /* Free the unescaped string as we are done with it */
    Curl_unicodefree(unescaped);

    if(!ludp->lud_dn) {
      rc = LDAP_NO_MEMORY;

      goto quit;
    }
#else
    ludp->lud_dn = unescaped;
#endif
  }

  p = q;
  if(!p)
    goto quit;

  /* Parse the attributes. skip "??" */
  q = strchr(p, '?');
  if(q)
    *q++ = '\0';

  if(*p) {
    char **attributes;
    size_t count = 0;

    /* Split the string into an array of attributes */
    if(!split_str(p, &attributes, &count)) {
      rc = LDAP_NO_MEMORY;

      goto quit;
    }

    /* Allocate our array (+1 for the NULL entry) */
#if defined(USE_WIN32_LDAP)
    ludp->lud_attrs = calloc(count + 1, sizeof(TCHAR *));
#else
    ludp->lud_attrs = calloc(count + 1, sizeof(char *));
#endif
    if(!ludp->lud_attrs) {
      free(attributes);

      rc = LDAP_NO_MEMORY;

      goto quit;
    }

    for(i = 0; i < count; i++) {
      char *unescaped;
      CURLcode result;

      LDAP_TRACE(("attr[%d] '%s'\n", i, attributes[i]));

      /* Unescape the attribute */
      result = Curl_urldecode(conn->data, attributes[i], 0, &unescaped, NULL,
                              FALSE);
      if(result) {
        free(attributes);

        rc = LDAP_NO_MEMORY;

        goto quit;
      }

#if defined(USE_WIN32_LDAP)
      /* Convert the unescaped string to a tchar */
      ludp->lud_attrs[i] = Curl_convert_UTF8_to_tchar(unescaped);

      /* Free the unescaped string as we are done with it */
      Curl_unicodefree(unescaped);

      if(!ludp->lud_attrs[i]) {
        free(attributes);

        rc = LDAP_NO_MEMORY;

        goto quit;
      }
#else
      ludp->lud_attrs[i] = unescaped;
#endif

      ludp->lud_attrs_dups++;
    }

    free(attributes);
  }

  p = q;
  if(!p)
    goto quit;

  /* Parse the scope. skip "??" */
  q = strchr(p, '?');
  if(q)
    *q++ = '\0';

  if(*p) {
    ludp->lud_scope = str2scope(p);
    if(ludp->lud_scope == -1) {
      rc = LDAP_INVALID_SYNTAX;

      goto quit;
    }
    LDAP_TRACE(("scope %d\n", ludp->lud_scope));
  }

  p = q;
  if(!p)
    goto quit;

  /* Parse the filter */
  q = strchr(p, '?');
  if(q)
    *q++ = '\0';

  if(*p) {
    char *filter = p;
    char *unescaped;
    CURLcode result;

    LDAP_TRACE(("filter '%s'\n", filter));

    /* Unescape the filter */
    result = Curl_urldecode(conn->data, filter, 0, &unescaped, NULL, FALSE);
    if(result) {
      rc = LDAP_NO_MEMORY;

      goto quit;
    }

#if defined(USE_WIN32_LDAP)
    /* Convert the unescaped string to a tchar */
    ludp->lud_filter = Curl_convert_UTF8_to_tchar(unescaped);

    /* Free the unescaped string as we are done with it */
    Curl_unicodefree(unescaped);

    if(!ludp->lud_filter) {
      rc = LDAP_NO_MEMORY;

      goto quit;
    }
#else
    ludp->lud_filter = unescaped;
#endif
  }

  p = q;
  if(p && !*p) {
    rc = LDAP_INVALID_SYNTAX;

    goto quit;
  }

quit:
  free(path);

  return rc;
}

static int _ldap_url_parse(const struct connectdata *conn,
                           LDAPURLDesc **ludpp)
{
  LDAPURLDesc *ludp = calloc(1, sizeof(*ludp));
  int rc;

  *ludpp = NULL;
  if(!ludp)
     return LDAP_NO_MEMORY;

  rc = _ldap_url_parse2(conn, ludp);
  if(rc != LDAP_SUCCESS) {
    _ldap_free_urldesc(ludp);
    ludp = NULL;
  }
  *ludpp = ludp;
  return (rc);
}

static void _ldap_free_urldesc(LDAPURLDesc *ludp)
{
  size_t i;

  if(!ludp)
    return;

  free(ludp->lud_dn);
  free(ludp->lud_filter);

  if(ludp->lud_attrs) {
    for(i = 0; i < ludp->lud_attrs_dups; i++)
      free(ludp->lud_attrs[i]);
    free(ludp->lud_attrs);
  }

  free(ludp);
}
#endif  /* !HAVE_LDAP_URL_PARSE */
#endif  /* !CURL_DISABLE_LDAP && !USE_OPENLDAP */
