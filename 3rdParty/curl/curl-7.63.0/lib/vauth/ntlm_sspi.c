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

#include "curl_setup.h"

#if defined(USE_WINDOWS_SSPI) && defined(USE_NTLM)

#include <curl/curl.h>

#include "vauth/vauth.h"
#include "urldata.h"
#include "curl_base64.h"
#include "curl_ntlm_core.h"
#include "warnless.h"
#include "curl_multibyte.h"
#include "sendf.h"

/* The last #include files should be: */
#include "curl_memory.h"
#include "memdebug.h"

/*
 * Curl_auth_is_ntlm_supported()
 *
 * This is used to evaluate if NTLM is supported.
 *
 * Parameters: None
 *
 * Returns TRUE if NTLM is supported by Windows SSPI.
 */
bool Curl_auth_is_ntlm_supported(void)
{
  PSecPkgInfo SecurityPackage;
  SECURITY_STATUS status;

  /* Query the security package for NTLM */
  status = s_pSecFn->QuerySecurityPackageInfo((TCHAR *) TEXT(SP_NAME_NTLM),
                                              &SecurityPackage);

  return (status == SEC_E_OK ? TRUE : FALSE);
}

/*
 * Curl_auth_create_ntlm_type1_message()
 *
 * This is used to generate an already encoded NTLM type-1 message ready for
 * sending to the recipient.
 *
 * Parameters:
 *
 * data    [in]     - The session handle.
 * userp   [in]     - The user name in the format User or Domain\User.
 * passwdp [in]     - The user's password.
 * service [in]     - The service type such as http, smtp, pop or imap.
 * host    [in]     - The host name.
 * ntlm    [in/out] - The NTLM data struct being used and modified.
 * outptr  [in/out] - The address where a pointer to newly allocated memory
 *                    holding the result will be stored upon completion.
 * outlen  [out]    - The length of the output message.
 *
 * Returns CURLE_OK on success.
 */
CURLcode Curl_auth_create_ntlm_type1_message(struct Curl_easy *data,
                                             const char *userp,
                                             const char *passwdp,
                                             const char *service,
                                             const char *host,
                                             struct ntlmdata *ntlm,
                                             char **outptr, size_t *outlen)
{
  PSecPkgInfo SecurityPackage;
  SecBuffer type_1_buf;
  SecBufferDesc type_1_desc;
  SECURITY_STATUS status;
  unsigned long attrs;
  TimeStamp expiry; /* For Windows 9x compatibility of SSPI calls */

  /* Clean up any former leftovers and initialise to defaults */
  Curl_auth_ntlm_cleanup(ntlm);

  /* Query the security package for NTLM */
  status = s_pSecFn->QuerySecurityPackageInfo((TCHAR *) TEXT(SP_NAME_NTLM),
                                              &SecurityPackage);
  if(status != SEC_E_OK)
    return CURLE_NOT_BUILT_IN;

  ntlm->token_max = SecurityPackage->cbMaxToken;

  /* Release the package buffer as it is not required anymore */
  s_pSecFn->FreeContextBuffer(SecurityPackage);

  /* Allocate our output buffer */
  ntlm->output_token = malloc(ntlm->token_max);
  if(!ntlm->output_token)
    return CURLE_OUT_OF_MEMORY;

  if(userp && *userp) {
    CURLcode result;

    /* Populate our identity structure */
    result = Curl_create_sspi_identity(userp, passwdp, &ntlm->identity);
    if(result)
      return result;

    /* Allow proper cleanup of the identity structure */
    ntlm->p_identity = &ntlm->identity;
  }
  else
    /* Use the current Windows user */
    ntlm->p_identity = NULL;

  /* Allocate our credentials handle */
  ntlm->credentials = calloc(1, sizeof(CredHandle));
  if(!ntlm->credentials)
    return CURLE_OUT_OF_MEMORY;

  /* Acquire our credentials handle */
  status = s_pSecFn->AcquireCredentialsHandle(NULL,
                                              (TCHAR *) TEXT(SP_NAME_NTLM),
                                              SECPKG_CRED_OUTBOUND, NULL,
                                              ntlm->p_identity, NULL, NULL,
                                              ntlm->credentials, &expiry);
  if(status != SEC_E_OK)
    return CURLE_LOGIN_DENIED;

  /* Allocate our new context handle */
  ntlm->context = calloc(1, sizeof(CtxtHandle));
  if(!ntlm->context)
    return CURLE_OUT_OF_MEMORY;

  ntlm->spn = Curl_auth_build_spn(service, host, NULL);
  if(!ntlm->spn)
    return CURLE_OUT_OF_MEMORY;

  /* Setup the type-1 "output" security buffer */
  type_1_desc.ulVersion = SECBUFFER_VERSION;
  type_1_desc.cBuffers  = 1;
  type_1_desc.pBuffers  = &type_1_buf;
  type_1_buf.BufferType = SECBUFFER_TOKEN;
  type_1_buf.pvBuffer   = ntlm->output_token;
  type_1_buf.cbBuffer   = curlx_uztoul(ntlm->token_max);

  /* Generate our type-1 message */
  status = s_pSecFn->InitializeSecurityContext(ntlm->credentials, NULL,
                                               ntlm->spn,
                                               0, 0, SECURITY_NETWORK_DREP,
                                               NULL, 0,
                                               ntlm->context, &type_1_desc,
                                               &attrs, &expiry);
  if(status == SEC_I_COMPLETE_NEEDED ||
    status == SEC_I_COMPLETE_AND_CONTINUE)
    s_pSecFn->CompleteAuthToken(ntlm->context, &type_1_desc);
  else if(status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED)
    return CURLE_RECV_ERROR;

  /* Base64 encode the response */
  return Curl_base64_encode(data, (char *) ntlm->output_token,
                            type_1_buf.cbBuffer, outptr, outlen);
}

/*
 * Curl_auth_decode_ntlm_type2_message()
 *
 * This is used to decode an already encoded NTLM type-2 message.
 *
 * Parameters:
 *
 * data     [in]     - The session handle.
 * type2msg [in]     - The base64 encoded type-2 message.
 * ntlm     [in/out] - The NTLM data struct being used and modified.
 *
 * Returns CURLE_OK on success.
 */
CURLcode Curl_auth_decode_ntlm_type2_message(struct Curl_easy *data,
                                             const char *type2msg,
                                             struct ntlmdata *ntlm)
{
  CURLcode result = CURLE_OK;
  unsigned char *type2 = NULL;
  size_t type2_len = 0;

#if defined(CURL_DISABLE_VERBOSE_STRINGS)
  (void) data;
#endif

  /* Decode the base-64 encoded type-2 message */
  if(strlen(type2msg) && *type2msg != '=') {
    result = Curl_base64_decode(type2msg, &type2, &type2_len);
    if(result)
      return result;
  }

  /* Ensure we have a valid type-2 message */
  if(!type2) {
    infof(data, "NTLM handshake failure (empty type-2 message)\n");

    return CURLE_BAD_CONTENT_ENCODING;
  }

  /* Simply store the challenge for use later */
  ntlm->input_token = type2;
  ntlm->input_token_len = type2_len;

  return result;
}

/*
* Curl_auth_create_ntlm_type3_message()
 * Curl_auth_create_ntlm_type3_message()
 *
 * This is used to generate an already encoded NTLM type-3 message ready for
 * sending to the recipient.
 *
 * Parameters:
 *
 * data    [in]     - The session handle.
 * userp   [in]     - The user name in the format User or Domain\User.
 * passwdp [in]     - The user's password.
 * ntlm    [in/out] - The NTLM data struct being used and modified.
 * outptr  [in/out] - The address where a pointer to newly allocated memory
 *                    holding the result will be stored upon completion.
 * outlen  [out]    - The length of the output message.
 *
 * Returns CURLE_OK on success.
 */
CURLcode Curl_auth_create_ntlm_type3_message(struct Curl_easy *data,
                                             const char *userp,
                                             const char *passwdp,
                                             struct ntlmdata *ntlm,
                                             char **outptr, size_t *outlen)
{
  CURLcode result = CURLE_OK;
  SecBuffer type_2_buf;
  SecBuffer type_3_buf;
  SecBufferDesc type_2_desc;
  SecBufferDesc type_3_desc;
  SECURITY_STATUS status;
  unsigned long attrs;
  TimeStamp expiry; /* For Windows 9x compatibility of SSPI calls */

  (void) passwdp;
  (void) userp;

  /* Setup the type-2 "input" security buffer */
  type_2_desc.ulVersion = SECBUFFER_VERSION;
  type_2_desc.cBuffers  = 1;
  type_2_desc.pBuffers  = &type_2_buf;
  type_2_buf.BufferType = SECBUFFER_TOKEN;
  type_2_buf.pvBuffer   = ntlm->input_token;
  type_2_buf.cbBuffer   = curlx_uztoul(ntlm->input_token_len);

  /* Setup the type-3 "output" security buffer */
  type_3_desc.ulVersion = SECBUFFER_VERSION;
  type_3_desc.cBuffers  = 1;
  type_3_desc.pBuffers  = &type_3_buf;
  type_3_buf.BufferType = SECBUFFER_TOKEN;
  type_3_buf.pvBuffer   = ntlm->output_token;
  type_3_buf.cbBuffer   = curlx_uztoul(ntlm->token_max);

  /* Generate our type-3 message */
  status = s_pSecFn->InitializeSecurityContext(ntlm->credentials,
                                               ntlm->context,
                                               ntlm->spn,
                                               0, 0, SECURITY_NETWORK_DREP,
                                               &type_2_desc,
                                               0, ntlm->context,
                                               &type_3_desc,
                                               &attrs, &expiry);
  if(status != SEC_E_OK) {
    infof(data, "NTLM handshake failure (type-3 message): Status=%x\n",
          status);

    return CURLE_RECV_ERROR;
  }

  /* Base64 encode the response */
  result = Curl_base64_encode(data, (char *) ntlm->output_token,
                              type_3_buf.cbBuffer, outptr, outlen);

  Curl_auth_ntlm_cleanup(ntlm);

  return result;
}

/*
 * Curl_auth_ntlm_cleanup()
 *
 * This is used to clean up the NTLM specific data.
 *
 * Parameters:
 *
 * ntlm    [in/out] - The NTLM data struct being cleaned up.
 *
 */
void Curl_auth_ntlm_cleanup(struct ntlmdata *ntlm)
{
  /* Free our security context */
  if(ntlm->context) {
    s_pSecFn->DeleteSecurityContext(ntlm->context);
    free(ntlm->context);
    ntlm->context = NULL;
  }

  /* Free our credentials handle */
  if(ntlm->credentials) {
    s_pSecFn->FreeCredentialsHandle(ntlm->credentials);
    free(ntlm->credentials);
    ntlm->credentials = NULL;
  }

  /* Free our identity */
  Curl_sspi_free_identity(ntlm->p_identity);
  ntlm->p_identity = NULL;

  /* Free the input and output tokens */
  Curl_safefree(ntlm->input_token);
  Curl_safefree(ntlm->output_token);

  /* Reset any variables */
  ntlm->token_max = 0;

  Curl_safefree(ntlm->spn);
}

#endif /* USE_WINDOWS_SSPI && USE_NTLM */
