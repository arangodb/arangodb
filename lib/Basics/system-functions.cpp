////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief memrchr
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_MISSING_MEMRCHR

void* memrchr(const void* block, int c, size_t size) {
  const unsigned char* p = static_cast<const unsigned char*>(block);

  if (size) {
    for (p += size - 1; size; p--, size--)
      if (*p == c) return (void*)p;
  }
  return NULL;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief get the time of day
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_GETTIMEOFDAY

int gettimeofday(struct timeval* tv, void* tz) {
  union {
    int64_t ns100;  // since 1.1.1601 in 100ns units
    FILETIME ft;
  } now;

  GetSystemTimeAsFileTime(&now.ft);

  tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
  tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL);

  return 0;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a line
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_HAVE_GETLINE

static int const line_size = 256;

ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
  size_t indx = 0;
  int c;

  // sanity checks
  if (lineptr == NULL || n == NULL || stream == NULL) {
    return -1;
  }

  // allocate the line the first time
  if (*lineptr == NULL) {
    *lineptr = (char*)TRI_SystemAllocate(line_size, false);

    if (*lineptr == NULL) {
      return -1;
    }

    *n = line_size;
  }

  // clear the line
  memset(*lineptr, '\0', *n);

  while ((c = getc(stream)) != EOF) {
    // check if more memory is needed
    if (indx >= *n) {
      *lineptr = (char*)realloc(*lineptr, *n + line_size);

      if (*lineptr == NULL) {
        return -1;
      }

      // clear the rest of the line
      memset(*lineptr + *n, '\0', line_size);
      *n += line_size;
    }

    // push the result in the line
    (*lineptr)[indx++] = c;

    // bail out
    if (c == '\n') {
      break;
    }
  }

  return (c == EOF) ? -1 : (ssize_t)indx;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief safe localtime
////////////////////////////////////////////////////////////////////////////////

void TRI_localtime(time_t tt, struct tm* tb) {
#ifdef TRI_HAVE_LOCALTIME_R

  localtime_r(&tt, tb);

#else

#ifdef TRI_HAVE_LOCALTIME_S

  localtime_s(tb, &tt);

#else

  struct tm* tp;

  tp = localtime(&tt);

  if (tp != NULL) {
    memcpy(tb, tp, sizeof(struct tm));
  }

#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief safe gmtime
////////////////////////////////////////////////////////////////////////////////

void TRI_gmtime(time_t tt, struct tm* tb) {
#ifdef TRI_HAVE_GMTIME_R

  gmtime_r(&tt, tb);

#else

#ifdef TRI_HAVE_GMTIME_S

  gmtime_s(tb, &tt);

#else

  struct tm* tp;

  tp = gmtime(&tt);

  if (tp != NULL) {
    memcpy(tb, tp, sizeof(struct tm));
  }

#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief seconds with microsecond resolution
////////////////////////////////////////////////////////////////////////////////

double TRI_microtime() {
  struct timeval t;

  gettimeofday(&t, 0);

  return (t.tv_sec) + (t.tv_usec / 1000000.0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief number of processors or 0
////////////////////////////////////////////////////////////////////////////////

size_t TRI_numberProcessors() {
#ifdef TRI_SC_NPROCESSORS_ONLN

  auto n = sysconf(_SC_NPROCESSORS_ONLN);

  if (n < 0) {
    n = 0;
  }

  return n;

#else

  return 0;

#endif
}
