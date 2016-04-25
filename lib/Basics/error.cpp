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
/// @brief error number and system error
////////////////////////////////////////////////////////////////////////////////

typedef struct tri_error_s {
  int _number;
  int _sys;
} tri_error_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialized
////////////////////////////////////////////////////////////////////////////////

static bool Initialized = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief holds the last error
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_GCC_THREAD_LOCAL_STORAGE
static __thread tri_error_t ErrorNumber;
#elif defined(TRI_WIN32_THREAD_LOCAL_STORAGE)
static __declspec(thread) tri_error_t ErrorNumber;
#elif defined(TRI_HAVE_POSIX_THREADS)
static pthread_key_t ErrorKey;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief the error messages
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<int, std::string> ErrorMessages;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_GCC_THREAD_LOCAL_STORAGE
#ifdef TRI_HAVE_POSIX_THREADS

static void CleanupError(void* ptr) { TRI_Free(TRI_CORE_MEM_ZONE, ptr); }

#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error
////////////////////////////////////////////////////////////////////////////////

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || \
    defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

int TRI_errno() { return ErrorNumber._number; }

#elif defined(TRI_HAVE_POSIX_THREADS)

int TRI_errno() {
  tri_error_t* eptr;

  eptr = static_cast<decltype(eptr)>(pthread_getspecific(ErrorKey));

  if (eptr == nullptr) {
    return 0;
  }
  return eptr->_number;
}

#else
#error no TLS
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error as string
////////////////////////////////////////////////////////////////////////////////

char const* TRI_last_error() {
  int err;
  int sys;

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || \
    defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

  err = ErrorNumber._number;
  sys = ErrorNumber._sys;

#elif defined(TRI_HAVE_POSIX_THREADS)

  tri_error_t* eptr = static_cast<decltype(eptr)>(pthread_getspecific(ErrorKey));

  if (eptr == nullptr) {
    err = 0;
    sys = 0;
  } else {
    err = eptr->_number;
    sys = eptr->_sys;
  }

#else
#error no TLS
#endif

  if (err == TRI_ERROR_SYS_ERROR) {
    return strerror(sys);
  }

  auto it = ErrorMessages.find(err);

  if (it == ErrorMessages.end()) {
    return "general error";
  }

  return (*it).second.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the last error
////////////////////////////////////////////////////////////////////////////////

int TRI_set_errno(int error) {
#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || \
    defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

  ErrorNumber._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    ErrorNumber._sys = errno;
  } else {
    ErrorNumber._sys = 0;
  }

#elif defined(TRI_HAVE_POSIX_THREADS)

  int copyErrno = errno;

  tri_error_t* eptr = static_cast<decltype(eptr)>(pthread_getspecific(ErrorKey));

  if (eptr == nullptr) {
    eptr = static_cast<decltype(eptr)>(
        TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(tri_error_t), false));
    pthread_setspecific(ErrorKey, eptr);
  }

  eptr->_number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    eptr->_sys = copyErrno;
  } else {
    eptr->_sys = 0;
  }

#else
#error no TLS
#endif

  return error;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an error string
////////////////////////////////////////////////////////////////////////////////

void TRI_set_errno_string(int code, char const* msg) {
  if (!ErrorMessages.emplace(code, std::string(msg)).second) {
    // logic error, error number is redeclared
    printf("Error: duplicate declaration of error code %i in %s:%i\n", code,
           __FILE__, __LINE__);
    TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an error message for an error code
////////////////////////////////////////////////////////////////////////////////

char const* TRI_errno_string(int code) {
  auto it = ErrorMessages.find(code);

  if (it == ErrorMessages.end()) {
    // return a hard-coded string as not all callers check for nullptr
    return "unknown error";
  }

  return (*it).second.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the error messages
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeError() {
  if (Initialized) {
    return;
  }

  TRI_InitializeErrorMessages();

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || \
    defined(TRI_WIN32_THREAD_LOCAL_STORAGE)
  ErrorNumber._number = 0;
  ErrorNumber._sys = 0;
#elif defined(TRI_HAVE_POSIX_THREADS)
  pthread_key_create(&ErrorKey, CleanupError);
#else
#error no TLS
#endif

  Initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the error messages
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownError() {
  Initialized = false;
}
