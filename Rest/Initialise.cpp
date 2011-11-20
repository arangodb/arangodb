////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <Rest/Initialise.h>

#include <build.h>

#include <Basics/Initialise.h>
#include <Basics/Logger.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define OPENSSL_THREAD_DEFINES

#include <openssl/opensslconf.h>

#ifndef OPENSSL_THREADS
#error missing thread support for openssl, please recomple OpenSSL with threads
#endif


#include "ResultGenerator/HtmlResultGenerator.h"
#include "ResultGenerator/JsonResultGenerator.h"
#include "ResultGenerator/JsonXResultGenerator.h"
#include "ResultGenerator/PhpResultGenerator.h"
#include "ResultGenerator/XmlResultGenerator.h"


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
    void InitialiseRest () {
      TRIAGENS_BASICS_INITIALISE;

      string revision = "$Revision: REST " TRIAGENS_VERSION " (c) triAGENS GmbH $";
      LOGGER_TRACE << revision;


      HtmlResultGenerator::initialise();
      JsonResultGenerator::initialise();
      JsonXResultGenerator::initialise();
      PhpResultGenerator::initialise();
      XmlResultGenerator::initialise();


      SSL_library_init();
      SSL_load_error_strings();
      OpenSSL_add_all_algorithms();
      ERR_load_crypto_strings();

      opensslSetup();
    }
  }
}

