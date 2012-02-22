////////////////////////////////////////////////////////////////////////////////
/// @brief error handling
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#ifndef TRI_GCC_THREAD_LOCAL_STORAGE
#ifdef TRI_HAVE_POSIX_THREADS
#include <pthread.h>
#endif
#endif

#include "BasicsC/strings.h"
#include "BasicsC/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ErrorHandling
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief error number and system error
////////////////////////////////////////////////////////////////////////////////

typedef struct tri_error_s {
  int _number;
  int _sys;
}
tri_error_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ErrorHandling
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

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

static TRI_vector_string_t ErrorMessages;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ErrorHandling
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_GCC_THREAD_LOCAL_STORAGE
#ifdef TRI_HAVE_POSIX_THREADS

static void CleanupError (void* ptr) {
  TRI_Free(ptr);
}

#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ErrorHandling
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error
////////////////////////////////////////////////////////////////////////////////

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

int TRI_errno () {
  return ErrorNumber._number;
}

#elif defined(TRI_HAVE_POSIX_THREADS)

int TRI_errno () {
  tri_error_t* eptr;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    return 0;
  }
  else {
    return eptr->_number;
  }
}

#else
#error no TLS
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error as string
////////////////////////////////////////////////////////////////////////////////

char const* TRI_last_error () {
  int err;
  int sys;

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

  err = ErrorNumber._number;
  sys = ErrorNumber._sys;

#elif defined(TRI_HAVE_POSIX_THREADS)

  tri_error_t* eptr;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    err = 0;
    sys = 0;
  }
  else {
    err = eptr->_number;
    sys = eptr->_sys;
  }

#else
#error no TLS
#endif

  if (err == TRI_ERROR_SYS_ERROR) {
    return strerror(sys);
  }

  if (err < (int) ErrorMessages._length) {
    char const* str = ErrorMessages._buffer[err];

    if (str == NULL) {
      return "general error";
    }

    return str;
  }

  return "general error";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the last error
////////////////////////////////////////////////////////////////////////////////

int TRI_set_errno (int error) {
#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || defined(TRI_WIN32_THREAD_LOCAL_STORAGE)

  ErrorNumber._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    ErrorNumber._sys = errno;
  }
  else {
    ErrorNumber._sys = 0;
  }

#elif defined(TRI_HAVE_POSIX_THREADS)

  tri_error_t* eptr;
  int copyErrno = errno;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    eptr = TRI_Allocate(sizeof(tri_error_t));
    pthread_setspecific(ErrorKey, eptr);
  }

  eptr->_number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    eptr->_sys = copyErrno;
  }
  else {
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

void TRI_set_errno_string (int error, char const* msg) {
  if (error >= (int) ErrorMessages._length) {
    TRI_ResizeVectorString(&ErrorMessages, error + 1);
  }

  ErrorMessages._buffer[error] = TRI_DuplicateString(msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ErrorHandling
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the error messages
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseError () {
  if (Initialised) {
    return;
  }

  TRI_InitVectorString(&ErrorMessages);

  TRI_set_errno_string(0, "no error");
  TRI_set_errno_string(1, "failed");
  TRI_set_errno_string(2, "system error");
  TRI_set_errno_string(3, "illegal number");
  TRI_set_errno_string(4, "numeric overflow");
  TRI_set_errno_string(5, "illegal option");
  TRI_set_errno_string(6, "dead process identifier");
  TRI_set_errno_string(7, "unlocked file");

#if defined(TRI_GCC_THREAD_LOCAL_STORAGE) || defined(TRI_WIN32_THREAD_LOCAL_STORAGE)
  ErrorNumber._number = 0;
  ErrorNumber._sys = 0;
#elif defined(TRI_HAVE_POSIX_THREADS)
  pthread_key_create(&ErrorKey, CleanupError);
#else
#error no TLS
#endif

  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the error messages
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownError () {
  if (! Initialised) {
    return;
  }

  TRI_DestroyVectorString(&ErrorMessages);

  Initialised = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
