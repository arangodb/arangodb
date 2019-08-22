/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
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
#include "curlcheck.h"

#include "curl_md5.h"

static CURLcode unit_setup(void)
{
  return CURLE_OK;
}

static void unit_stop(void)
{

}

UNITTEST_START

#ifndef CURL_DISABLE_CRYPTO_AUTH
  unsigned char output[16];
  unsigned char *testp = output;
  Curl_md5it(output, (const unsigned char *)"1");

/* !checksrc! disable LONGLINE 2 */
  verify_memory(testp,
                "\xc4\xca\x42\x38\xa0\xb9\x23\x82\x0d\xcc\x50\x9a\x6f\x75\x84\x9b", 16);

  Curl_md5it(output, (const unsigned char *)"hello-you-fool");

  verify_memory(testp,
                "\x88\x67\x0b\x6d\x5d\x74\x2f\xad\xa5\xcd\xf9\xb6\x82\x87\x5f\x22", 16);
#endif


UNITTEST_STOP
