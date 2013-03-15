////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "InitialiseRest.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#define OPENSSL_THREAD_DEFINES

#include <openssl/opensslconf.h>

#ifndef OPENSSL_THREADS
#error missing thread support for openssl, please recomple OpenSSL with threads
#endif

#include "build.h"

#include "Basics/InitialiseBasics.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Rest/Url.h"
#include "Statistics/statistics.h"

// -----------------------------------------------------------------------------
// OPEN SSL support
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_POSIX_THREADS

namespace {
  long* opensslLockCount;
  pthread_mutex_t* opensslLocks;



  unsigned long opensslThreadId () {
    return (unsigned long) pthread_self();
  }



  void opensslLockingCallback (int mode, int type, char const* /* file */, int /* line */) {
    if (mode & CRYPTO_LOCK) {
      pthread_mutex_lock(&(opensslLocks[type]));
      opensslLockCount[type]++;
    }
    else {
      pthread_mutex_unlock(&(opensslLocks[type]));
    }

  }



  void opensslSetup () {
    opensslLockCount = (long*) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
    opensslLocks = (pthread_mutex_t*) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));

    for (long i = 0;  i < CRYPTO_num_locks();  ++i) {
      opensslLockCount[i] = 0;
      pthread_mutex_init(&(opensslLocks[i]), 0);
    }

    CRYPTO_set_id_callback(opensslThreadId);
    CRYPTO_set_locking_callback(opensslLockingCallback);
  }



  void opensslCleanup () {
    CRYPTO_set_locking_callback(0);

    for (long i = 0;  i < CRYPTO_num_locks();  ++i) {
      pthread_mutex_destroy(&(opensslLocks[i]));
    }

    OPENSSL_free(opensslLocks);
    OPENSSL_free(opensslLockCount);
  }
}

#endif

// -----------------------------------------------------------------------------
// initialisation
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    void InitialiseRest (int argc, char* argv[]) {
      TRIAGENS_BASICS_INITIALISE(argc, argv);

      TRI_InitialiseUrl();
      TRI_InitialiseStatistics();

      string revision = "$Revision: REST " TRIAGENS_VERSION " (c) triAGENS GmbH $";
      LOGGER_TRACE(revision);

      SSL_library_init();
      SSL_load_error_strings();
      OpenSSL_add_all_algorithms();
      ERR_load_crypto_strings();

#ifdef TRI_OPENSSL_VERSION
      revision = "$Revision: OPENSSL " TRI_OPENSSL_VERSION " $";
      LOGGER_TRACE(revision);
#endif

#ifdef TRI_LIBEV_VERSION
      revision = "$Revision: LIBEV " TRI_LIBEV_VERSION " $";
      LOGGER_TRACE(revision);
#endif

#ifdef TRI_HAVE_POSIX_THREADS
      opensslSetup();
#endif

      Version::initialise();
    }



    void ShutdownRest () {
#ifdef TRI_HAVE_POSIX_THREADS
        opensslCleanup();
#endif

      TRI_ShutdownUrl();

      TRIAGENS_BASICS_SHUTDOWN;
    }
  }
}

