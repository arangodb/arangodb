////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
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

#include "Basics/logging.h"

#include "Basics/InitialiseBasics.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Statistics/statistics.h"

// -----------------------------------------------------------------------------
// OPEN SSL support
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_POSIX_THREADS

namespace {
  long* opensslLockCount;
  pthread_mutex_t* opensslLocks;

#if OPENSSL_VERSION_NUMBER < 0x01000000L

  unsigned long opensslThreadId () {
    return (unsigned long) pthread_self();
  }

#else
  // The compiler chooses the right one from the following two,
  // according to the type of the return value of pthread_self():

  template<typename T> void setter (CRYPTO_THREADID* id, T p) {
    CRYPTO_THREADID_set_pointer(id, p);
  }
  
  template<> void setter (CRYPTO_THREADID* id, unsigned long val) {
    CRYPTO_THREADID_set_numeric(id, val);
  }

  static void arango_threadid_func (CRYPTO_THREADID *id) {
    auto self = pthread_self();

    setter<decltype(self)>(id, self);
  }

#endif

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

#if OPENSSL_VERSION_NUMBER < 0x01000000L
    CRYPTO_set_id_callback(opensslThreadId);
    CRYPTO_set_locking_callback(opensslLockingCallback);
#else
    CRYPTO_THREADID_set_callback(arango_threadid_func);
    CRYPTO_set_locking_callback(opensslLockingCallback);
#endif
  }

  void opensslCleanup () {
    CRYPTO_set_locking_callback(nullptr);

#if OPENSSL_VERSION_NUMBER < 0x01000000L
    CRYPTO_set_id_callback(nullptr);
#else
    CRYPTO_THREADID_set_callback(nullptr);
#endif

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

      TRI_InitialiseStatistics();

      SSL_library_init();
      SSL_load_error_strings();
      OpenSSL_add_all_algorithms();
      ERR_load_crypto_strings();

#ifdef TRI_HAVE_POSIX_THREADS
      opensslSetup();
#endif

      Version::initialise();
    }



    void ShutdownRest () {
#ifdef TRI_HAVE_POSIX_THREADS
        opensslCleanup();
#endif

      TRI_ShutdownStatistics();

      TRIAGENS_BASICS_SHUTDOWN;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
