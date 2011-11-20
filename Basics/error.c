////////////////////////////////////////////////////////////////////////////////
/// @brief error handling
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Common.h>

#ifndef TRI_GCC_THREAD_LOCAL_STORAGE
#include <pthread.h>
#endif

#include <Basics/strings.h>
#include <Basics/vector.h>

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
/// @brief pthread local storage key
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_GCC_THREAD_LOCAL_STORAGE
static pthread_key_t ErrorKey;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief holds the last error
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_GCC_THREAD_LOCAL_STORAGE
static __thread tri_error_t ErrorNumber;
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

static void CleanupError (void* ptr) {
  TRI_Free(ptr);
}

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

int TRI_errno () {
#ifdef TRI_GCC_THREAD_LOCAL_STORAGE

  return ErrorNumber._number;

#else

  tri_error_t* eptr;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    return 0;
  }
  else {
    return eptr->_number;
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last error as string
////////////////////////////////////////////////////////////////////////////////

char const* TRI_last_error () {
  int err;
  int sys;

#ifdef TRI_GCC_THREAD_LOCAL_STORAGE

  err = ErrorNumber._number;
  sys = ErrorNumber._sys;

#else

  tri_error_t* eptr;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    err = 0;
    sys = 0;
  }
  else {
    err = eptr->_number;
    err = eptr->_sys;
  }

#endif

  if (err == TRI_ERROR_SYS_ERROR) {
    return strerror(sys);
  }

  if (err < TRI_SizeVectorString(&ErrorMessages)) {
    char const* str = TRI_AtVectorString(&ErrorMessages, err);

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
#ifdef TRI_GCC_THREAD_LOCAL_STORAGE

  ErrorNumber._number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    ErrorNumber._sys = errno;
  }
  else {
    ErrorNumber._sys = 0;
  }

#else

  tri_error_t* eptr;

  eptr = pthread_getspecific(ErrorKey);

  if (eptr == NULL) {
    eptr = TRI_Allocate(sizeof(tri_error_t));
    pthread_setspecific(ErrorKey, eptr);
  }

  eptr->_number = error;

  if (error == TRI_ERROR_SYS_ERROR) {
    eptr->_sys = errno;
  }
  else {
    eptr->_sys = 0;
  }

#endif

  return error;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an error string
////////////////////////////////////////////////////////////////////////////////

void TRI_set_errno_string (int error, char const* msg) {
  if (error >= TRI_SizeVectorString(&ErrorMessages)) {
    TRI_ResizeVectorString(&ErrorMessages, error + 1);
  }

  TRI_SetVectorString(&ErrorMessages, error, TRI_DuplicateString(msg));
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
  TRI_InitVectorString(&ErrorMessages);

  TRI_set_errno_string(0, "no error");
  TRI_set_errno_string(1, "failed");
  TRI_set_errno_string(2, "system error");

#ifdef TRI_GCC_THREAD_LOCAL_STORAGE
  ErrorNumber._number = 0;
  ErrorNumber._sys = 0;
#else
  pthread_key_create(&ErrorKey, CleanupError);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the error messages
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownError () {
  TRI_DestroyVectorString(&ErrorMessages);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
