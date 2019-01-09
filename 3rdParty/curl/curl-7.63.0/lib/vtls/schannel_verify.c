/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2012 - 2016, Marc Hoersken, <info@marc-hoersken.de>
 * Copyright (C) 2012, Mark Salisbury, <mark.salisbury@hp.com>
 * Copyright (C) 2012 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al.
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
 * Source file for Schannel-specific certificate verification. This code should
 * only be invoked by code in schannel.c.
 */

#include "curl_setup.h"

#ifdef USE_SCHANNEL
#ifndef USE_WINDOWS_SSPI
#  error "Can't compile SCHANNEL support without SSPI."
#endif

#define EXPOSE_SCHANNEL_INTERNAL_STRUCTS
#include "schannel.h"

#ifdef HAS_MANUAL_VERIFY_API

#include "vtls.h"
#include "sendf.h"
#include "strerror.h"
#include "curl_multibyte.h"
#include "curl_printf.h"
#include "hostcheck.h"
#include "system_win32.h"

/* The last #include file should be: */
#include "curl_memory.h"
#include "memdebug.h"

#define BACKEND connssl->backend

#define MAX_CAFILE_SIZE 1048576 /* 1 MiB */
#define BEGIN_CERT "-----BEGIN CERTIFICATE-----"
#define END_CERT "\n-----END CERTIFICATE-----"

typedef struct {
  DWORD cbSize;
  HCERTSTORE hRestrictedRoot;
  HCERTSTORE hRestrictedTrust;
  HCERTSTORE hRestrictedOther;
  DWORD cAdditionalStore;
  HCERTSTORE *rghAdditionalStore;
  DWORD dwFlags;
  DWORD dwUrlRetrievalTimeout;
  DWORD MaximumCachedCertificates;
  DWORD CycleDetectionModulus;
  HCERTSTORE hExclusiveRoot;
  HCERTSTORE hExclusiveTrustedPeople;
} CERT_CHAIN_ENGINE_CONFIG_WIN7, *PCERT_CHAIN_ENGINE_CONFIG_WIN7;

static int is_cr_or_lf(char c)
{
  return c == '\r' || c == '\n';
}

static CURLcode add_certs_to_store(HCERTSTORE trust_store,
                                   const char *ca_file,
                                   struct connectdata *conn)
{
  CURLcode result;
  struct Curl_easy *data = conn->data;
  HANDLE ca_file_handle = INVALID_HANDLE_VALUE;
  LARGE_INTEGER file_size;
  char *ca_file_buffer = NULL;
  char *current_ca_file_ptr = NULL;
  const TCHAR *ca_file_tstr = NULL;
  size_t ca_file_bufsize = 0;
  DWORD total_bytes_read = 0;
  bool more_certs = 0;
  int num_certs = 0;
  size_t END_CERT_LEN;

  ca_file_tstr = Curl_convert_UTF8_to_tchar(ca_file);
  if(!ca_file_tstr) {
    failf(data,
          "schannel: invalid path name for CA file '%s': %s",
          ca_file, Curl_strerror(conn, GetLastError()));
    result = CURLE_SSL_CACERT_BADFILE;
    goto cleanup;
  }

  /*
   * Read the CA file completely into memory before parsing it. This
   * optimizes for the common case where the CA file will be relatively
   * small ( < 1 MiB ).
   */
  ca_file_handle = CreateFile(ca_file_tstr,
                              GENERIC_READ,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
  if(ca_file_handle == INVALID_HANDLE_VALUE) {
    failf(data,
          "schannel: failed to open CA file '%s': %s",
          ca_file, Curl_strerror(conn, GetLastError()));
    result = CURLE_SSL_CACERT_BADFILE;
    goto cleanup;
  }

  if(!GetFileSizeEx(ca_file_handle, &file_size)) {
    failf(data,
          "schannel: failed to determine size of CA file '%s': %s",
          ca_file, Curl_strerror(conn, GetLastError()));
    result = CURLE_SSL_CACERT_BADFILE;
    goto cleanup;
  }

  if(file_size.QuadPart > MAX_CAFILE_SIZE) {
    failf(data,
          "schannel: CA file exceeds max size of %u bytes",
          MAX_CAFILE_SIZE);
    result = CURLE_SSL_CACERT_BADFILE;
    goto cleanup;
  }

  ca_file_bufsize = (size_t)file_size.QuadPart;
  ca_file_buffer = (char *)malloc(ca_file_bufsize + 1);
  if(!ca_file_buffer) {
    result = CURLE_OUT_OF_MEMORY;
    goto cleanup;
  }

  result = CURLE_OK;
  while(total_bytes_read < ca_file_bufsize) {
    DWORD bytes_to_read = (DWORD)(ca_file_bufsize - total_bytes_read);
    DWORD bytes_read = 0;

    if(!ReadFile(ca_file_handle, ca_file_buffer + total_bytes_read,
                 bytes_to_read, &bytes_read, NULL)) {

      failf(data,
            "schannel: failed to read from CA file '%s': %s",
            ca_file, Curl_strerror(conn, GetLastError()));
      result = CURLE_SSL_CACERT_BADFILE;
      goto cleanup;
    }
    if(bytes_read == 0) {
      /* Premature EOF -- adjust the bufsize to the new value */
      ca_file_bufsize = total_bytes_read;
    }
    else {
      total_bytes_read += bytes_read;
    }
  }

  /* Null terminate the buffer */
  ca_file_buffer[ca_file_bufsize] = '\0';

  if(result != CURLE_OK) {
    goto cleanup;
  }

  END_CERT_LEN = strlen(END_CERT);

  more_certs = 1;
  current_ca_file_ptr = ca_file_buffer;
  while(more_certs && *current_ca_file_ptr != '\0') {
    char *begin_cert_ptr = strstr(current_ca_file_ptr, BEGIN_CERT);
    if(!begin_cert_ptr || !is_cr_or_lf(begin_cert_ptr[strlen(BEGIN_CERT)])) {
      more_certs = 0;
    }
    else {
      char *end_cert_ptr = strstr(begin_cert_ptr, END_CERT);
      if(!end_cert_ptr) {
        failf(data,
              "schannel: CA file '%s' is not correctly formatted",
              ca_file);
        result = CURLE_SSL_CACERT_BADFILE;
        more_certs = 0;
      }
      else {
        CERT_BLOB cert_blob;
        CERT_CONTEXT *cert_context = NULL;
        BOOL add_cert_result = FALSE;
        DWORD actual_content_type = 0;
        DWORD cert_size = (DWORD)
          ((end_cert_ptr + END_CERT_LEN) - begin_cert_ptr);

        cert_blob.pbData = (BYTE *)begin_cert_ptr;
        cert_blob.cbData = cert_size;
        if(!CryptQueryObject(CERT_QUERY_OBJECT_BLOB,
                             &cert_blob,
                             CERT_QUERY_CONTENT_FLAG_CERT,
                             CERT_QUERY_FORMAT_FLAG_ALL,
                             0,
                             NULL,
                             &actual_content_type,
                             NULL,
                             NULL,
                             NULL,
                             (const void **)&cert_context)) {

          failf(data,
                "schannel: failed to extract certificate from CA file "
                "'%s': %s",
                ca_file, Curl_strerror(conn, GetLastError()));
          result = CURLE_SSL_CACERT_BADFILE;
          more_certs = 0;
        }
        else {
          current_ca_file_ptr = begin_cert_ptr + cert_size;

          /* Sanity check that the cert_context object is the right type */
          if(CERT_QUERY_CONTENT_CERT != actual_content_type) {
            failf(data,
                  "schannel: unexpected content type '%d' when extracting "
                  "certificate from CA file '%s'",
                  actual_content_type, ca_file);
            result = CURLE_SSL_CACERT_BADFILE;
            more_certs = 0;
          }
          else {
            add_cert_result =
              CertAddCertificateContextToStore(trust_store,
                                               cert_context,
                                               CERT_STORE_ADD_ALWAYS,
                                               NULL);
            CertFreeCertificateContext(cert_context);
            if(!add_cert_result) {
              failf(data,
                    "schannel: failed to add certificate from CA file '%s' "
                    "to certificate store: %s",
                    ca_file, Curl_strerror(conn, GetLastError()));
              result = CURLE_SSL_CACERT_BADFILE;
              more_certs = 0;
            }
            else {
              num_certs++;
            }
          }
        }
      }
    }
  }

  if(result == CURLE_OK) {
    if(!num_certs) {
      infof(data,
            "schannel: did not add any certificates from CA file '%s'\n",
            ca_file);
    }
    else {
      infof(data,
            "schannel: added %d certificate(s) from CA file '%s'\n",
            num_certs, ca_file);
    }
  }

cleanup:
  if(ca_file_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(ca_file_handle);
  }
  Curl_safefree(ca_file_buffer);
  Curl_unicodefree(ca_file_tstr);

  return result;
}

static CURLcode verify_host(struct Curl_easy *data,
                            CERT_CONTEXT *pCertContextServer,
                            const char * const conn_hostname)
{
  CURLcode result = CURLE_PEER_FAILED_VERIFICATION;
  TCHAR *cert_hostname_buff = NULL;
  size_t cert_hostname_buff_index = 0;
  DWORD len = 0;
  DWORD actual_len = 0;

  /* CertGetNameString will provide the 8-bit character string without
   * any decoding */
  DWORD name_flags = CERT_NAME_DISABLE_IE4_UTF8_FLAG;

#ifdef CERT_NAME_SEARCH_ALL_NAMES_FLAG
  name_flags |= CERT_NAME_SEARCH_ALL_NAMES_FLAG;
#endif

  /* Determine the size of the string needed for the cert hostname */
  len = CertGetNameString(pCertContextServer,
                          CERT_NAME_DNS_TYPE,
                          name_flags,
                          NULL,
                          NULL,
                          0);
  if(len == 0) {
    failf(data,
          "schannel: CertGetNameString() returned no "
          "certificate name information");
    result = CURLE_PEER_FAILED_VERIFICATION;
    goto cleanup;
  }

  /* CertGetNameString guarantees that the returned name will not contain
   * embedded null bytes. This appears to be undocumented behavior.
   */
  cert_hostname_buff = (LPTSTR)malloc(len * sizeof(TCHAR));
  if(!cert_hostname_buff) {
    result = CURLE_OUT_OF_MEMORY;
    goto cleanup;
  }
  actual_len = CertGetNameString(pCertContextServer,
                                 CERT_NAME_DNS_TYPE,
                                 name_flags,
                                 NULL,
                                 (LPTSTR) cert_hostname_buff,
                                 len);

  /* Sanity check */
  if(actual_len != len) {
    failf(data,
          "schannel: CertGetNameString() returned certificate "
          "name information of unexpected size");
    result = CURLE_PEER_FAILED_VERIFICATION;
    goto cleanup;
  }

  /* If HAVE_CERT_NAME_SEARCH_ALL_NAMES is available, the output
   * will contain all DNS names, where each name is null-terminated
   * and the last DNS name is double null-terminated. Due to this
   * encoding, use the length of the buffer to iterate over all names.
   */
  result = CURLE_PEER_FAILED_VERIFICATION;
  while(cert_hostname_buff_index < len &&
        cert_hostname_buff[cert_hostname_buff_index] != TEXT('\0') &&
        result == CURLE_PEER_FAILED_VERIFICATION) {

    char *cert_hostname;

    /* Comparing the cert name and the connection hostname encoded as UTF-8
     * is acceptable since both values are assumed to use ASCII
     * (or some equivalent) encoding
     */
    cert_hostname = Curl_convert_tchar_to_UTF8(
        &cert_hostname_buff[cert_hostname_buff_index]);
    if(!cert_hostname) {
      result = CURLE_OUT_OF_MEMORY;
    }
    else {
      int match_result;

      match_result = Curl_cert_hostcheck(cert_hostname, conn_hostname);
      if(match_result == CURL_HOST_MATCH) {
        infof(data,
              "schannel: connection hostname (%s) validated "
              "against certificate name (%s)\n",
              conn_hostname, cert_hostname);
        result = CURLE_OK;
      }
      else {
        size_t cert_hostname_len;

        infof(data,
              "schannel: connection hostname (%s) did not match "
              "against certificate name (%s)\n",
              conn_hostname, cert_hostname);

        cert_hostname_len = _tcslen(
            &cert_hostname_buff[cert_hostname_buff_index]);

        /* Move on to next cert name */
        cert_hostname_buff_index += cert_hostname_len + 1;

        result = CURLE_PEER_FAILED_VERIFICATION;
      }
      Curl_unicodefree(cert_hostname);
    }
  }

  if(result == CURLE_PEER_FAILED_VERIFICATION) {
    failf(data,
          "schannel: CertGetNameString() failed to match "
          "connection hostname (%s) against server certificate names",
          conn_hostname);
  }
  else if(result != CURLE_OK)
    failf(data, "schannel: server certificate name verification failed");

cleanup:
  Curl_unicodefree(cert_hostname_buff);

  return result;
}

CURLcode Curl_verify_certificate(struct connectdata *conn, int sockindex)
{
  SECURITY_STATUS status;
  struct Curl_easy *data = conn->data;
  struct ssl_connect_data *connssl = &conn->ssl[sockindex];
  CURLcode result = CURLE_OK;
  CERT_CONTEXT *pCertContextServer = NULL;
  const CERT_CHAIN_CONTEXT *pChainContext = NULL;
  HCERTCHAINENGINE cert_chain_engine = NULL;
  HCERTSTORE trust_store = NULL;
  const char * const conn_hostname = SSL_IS_PROXY() ?
    conn->http_proxy.host.name :
    conn->host.name;

  status = s_pSecFn->QueryContextAttributes(&BACKEND->ctxt->ctxt_handle,
                                            SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                            &pCertContextServer);

  if((status != SEC_E_OK) || (pCertContextServer == NULL)) {
    failf(data, "schannel: Failed to read remote certificate context: %s",
          Curl_sspi_strerror(conn, status));
    result = CURLE_PEER_FAILED_VERIFICATION;
  }

  if(result == CURLE_OK && SSL_CONN_CONFIG(CAfile) &&
      BACKEND->use_manual_cred_validation) {
    /*
     * Create a chain engine that uses the certificates in the CA file as
     * trusted certificates. This is only supported on Windows 7+.
     */

    if(Curl_verify_windows_version(6, 1, PLATFORM_WINNT, VERSION_LESS_THAN)) {
      failf(data, "schannel: this version of Windows is too old to support "
            "certificate verification via CA bundle file.");
      result = CURLE_SSL_CACERT_BADFILE;
    }
    else {
      /* Open the certificate store */
      trust_store = CertOpenStore(CERT_STORE_PROV_MEMORY,
                                  0,
                                  (HCRYPTPROV)NULL,
                                  CERT_STORE_CREATE_NEW_FLAG,
                                  NULL);
      if(!trust_store) {
        failf(data, "schannel: failed to create certificate store: %s",
              Curl_strerror(conn, GetLastError()));
        result = CURLE_SSL_CACERT_BADFILE;
      }
      else {
        result = add_certs_to_store(trust_store, SSL_CONN_CONFIG(CAfile),
                                    conn);
      }
    }

    if(result == CURLE_OK) {
      CERT_CHAIN_ENGINE_CONFIG_WIN7 engine_config;
      BOOL create_engine_result;

      memset(&engine_config, 0, sizeof(engine_config));
      engine_config.cbSize = sizeof(engine_config);
      engine_config.hExclusiveRoot = trust_store;

      /* CertCreateCertificateChainEngine will check the expected size of the
       * CERT_CHAIN_ENGINE_CONFIG structure and fail if the specified size
       * does not match the expected size. When this occurs, it indicates that
       * CAINFO is not supported on the version of Windows in use.
       */
      create_engine_result =
        CertCreateCertificateChainEngine(
          (CERT_CHAIN_ENGINE_CONFIG *)&engine_config, &cert_chain_engine);
      if(!create_engine_result) {
        failf(data,
              "schannel: failed to create certificate chain engine: %s",
              Curl_strerror(conn, GetLastError()));
        result = CURLE_SSL_CACERT_BADFILE;
      }
    }
  }

  if(result == CURLE_OK) {
    CERT_CHAIN_PARA ChainPara;

    memset(&ChainPara, 0, sizeof(ChainPara));
    ChainPara.cbSize = sizeof(ChainPara);

    if(!CertGetCertificateChain(cert_chain_engine,
                                pCertContextServer,
                                NULL,
                                pCertContextServer->hCertStore,
                                &ChainPara,
                                (data->set.ssl.no_revoke ? 0 :
                                 CERT_CHAIN_REVOCATION_CHECK_CHAIN),
                                NULL,
                                &pChainContext)) {
      failf(data, "schannel: CertGetCertificateChain failed: %s",
            Curl_sspi_strerror(conn, GetLastError()));
      pChainContext = NULL;
      result = CURLE_PEER_FAILED_VERIFICATION;
    }

    if(result == CURLE_OK) {
      CERT_SIMPLE_CHAIN *pSimpleChain = pChainContext->rgpChain[0];
      DWORD dwTrustErrorMask = ~(DWORD)(CERT_TRUST_IS_NOT_TIME_NESTED);
      dwTrustErrorMask &= pSimpleChain->TrustStatus.dwErrorStatus;
      if(dwTrustErrorMask) {
        if(dwTrustErrorMask & CERT_TRUST_IS_REVOKED)
          failf(data, "schannel: CertGetCertificateChain trust error"
                " CERT_TRUST_IS_REVOKED");
        else if(dwTrustErrorMask & CERT_TRUST_IS_PARTIAL_CHAIN)
          failf(data, "schannel: CertGetCertificateChain trust error"
                " CERT_TRUST_IS_PARTIAL_CHAIN");
        else if(dwTrustErrorMask & CERT_TRUST_IS_UNTRUSTED_ROOT)
          failf(data, "schannel: CertGetCertificateChain trust error"
                " CERT_TRUST_IS_UNTRUSTED_ROOT");
        else if(dwTrustErrorMask & CERT_TRUST_IS_NOT_TIME_VALID)
          failf(data, "schannel: CertGetCertificateChain trust error"
                " CERT_TRUST_IS_NOT_TIME_VALID");
        else if(dwTrustErrorMask & CERT_TRUST_REVOCATION_STATUS_UNKNOWN)
          failf(data, "schannel: CertGetCertificateChain trust error"
                " CERT_TRUST_REVOCATION_STATUS_UNKNOWN");
        else
          failf(data, "schannel: CertGetCertificateChain error mask: 0x%08x",
                dwTrustErrorMask);
        result = CURLE_PEER_FAILED_VERIFICATION;
      }
    }
  }

  if(result == CURLE_OK) {
    if(SSL_CONN_CONFIG(verifyhost)) {
      result = verify_host(conn->data, pCertContextServer, conn_hostname);
    }
  }

  if(cert_chain_engine) {
    CertFreeCertificateChainEngine(cert_chain_engine);
  }

  if(trust_store) {
    CertCloseStore(trust_store, 0);
  }

  if(pChainContext)
    CertFreeCertificateChain(pChainContext);

  if(pCertContextServer)
    CertFreeCertificateContext(pCertContextServer);

  return result;
}

#endif /* HAS_MANUAL_VERIFY_API */
#endif /* USE_SCHANNEL */
