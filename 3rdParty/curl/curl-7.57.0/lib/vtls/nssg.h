#ifndef HEADER_CURL_NSSG_H
#define HEADER_CURL_NSSG_H
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

#ifdef USE_NSS
/*
 * This header should only be needed to get included by vtls.c and nss.c
 */

#include "urldata.h"

/* initialize NSS library if not already */
CURLcode Curl_nss_force_init(struct Curl_easy *data);

extern const struct Curl_ssl Curl_ssl_nss;

#endif /* USE_NSS */
#endif /* HEADER_CURL_NSSG_H */
