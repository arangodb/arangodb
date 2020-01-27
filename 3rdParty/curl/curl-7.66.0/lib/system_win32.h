#ifndef HEADER_CURL_SYSTEM_WIN32_H
#define HEADER_CURL_SYSTEM_WIN32_H
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2016 - 2019, Steve Holme, <steve_holme@hotmail.com>.
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

#if defined(WIN32)

extern LARGE_INTEGER Curl_freq;
extern bool Curl_isVistaOrGreater;

CURLcode Curl_win32_init(long flags);
void Curl_win32_cleanup(long init_flags);

/* Version condition */
typedef enum {
  VERSION_LESS_THAN,
  VERSION_LESS_THAN_EQUAL,
  VERSION_EQUAL,
  VERSION_GREATER_THAN_EQUAL,
  VERSION_GREATER_THAN
} VersionCondition;

/* Platform identifier */
typedef enum {
  PLATFORM_DONT_CARE,
  PLATFORM_WINDOWS,
  PLATFORM_WINNT
} PlatformIdentifier;

/* We use our own typedef here since some headers might lack this */
typedef unsigned int(WINAPI *IF_NAMETOINDEX_FN)(const char *);

/* This is used instead of if_nametoindex if available on Windows */
extern IF_NAMETOINDEX_FN Curl_if_nametoindex;

/* This is used to verify if we are running on a specific windows version */
bool Curl_verify_windows_version(const unsigned int majorVersion,
                                 const unsigned int minorVersion,
                                 const PlatformIdentifier platform,
                                 const VersionCondition condition);

/* This is used to dynamically load DLLs */
HMODULE Curl_load_library(LPCTSTR filename);

#endif /* WIN32 */

#endif /* HEADER_CURL_SYSTEM_WIN32_H */
