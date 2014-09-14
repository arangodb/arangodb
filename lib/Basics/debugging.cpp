////////////////////////////////////////////////////////////////////////////////
/// @brief debugging helpers
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/locks.h"
#include "Basics/logging.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a global string containing the currently registered failure points
/// the string is a comma-separated list of point names
////////////////////////////////////////////////////////////////////////////////

static char* FailurePoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-write lock for thread-safe access to the failure-points list
////////////////////////////////////////////////////////////////////////////////

TRI_read_write_lock_t FailurePointsLock;

#ifdef TRI_ENABLE_FAILURE_TESTS

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief make a delimited value from a string, so we can unambigiously
/// search for it (e.g. searching for just "foo" would find "foo" and "foobar",
/// so we'll be putting the value inside some delimiter: ",foo,")
////////////////////////////////////////////////////////////////////////////////

static char* MakeValue (char const* value) {
  if (value == nullptr || strlen(value) == 0) {
    return nullptr;
  }

  char* delimited = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(value) + 3, false));

  if (delimited != nullptr) {
    memcpy(delimited + 1, value, strlen(value));
    delimited[0] = ',';
    delimited[strlen(value) + 1] = ',';
    delimited[strlen(value) + 2] = '\0';
  }

  return delimited;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cause a segmentation violation
/// this is used for crash and recovery tests
////////////////////////////////////////////////////////////////////////////////

void TRI_SegfaultDebugging (char const* message) {
  LOG_WARNING("%s: summon Baal!", message);
  // make sure the latest log messages are flushed
  TRI_ShutdownLogging(true);

  // and now crash
#ifndef __APPLE__
  // on MacOS, the following statement makes the server hang but not crash
  *((char*) -1) = '!';
#endif

  // ensure the process is terminated
  abort();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we should fail at a specific failure point
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShouldFailDebugging (char const* value) {
  char* found = nullptr;

  // try without the lock first (to speed things up)
  if (FailurePoints == nullptr) {
    return false;
  }

  TRI_ReadLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints != nullptr) {
    char* checkValue = MakeValue(value);

    if (checkValue != nullptr) {
      found = strstr(FailurePoints, checkValue);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
    }
  }

  TRI_ReadUnlockReadWriteLock(&FailurePointsLock);

  return (found != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a failure point
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFailurePointDebugging (char const* value) {
  char* found;
  char* checkValue;

  checkValue = MakeValue(value);

  if (checkValue == nullptr) {
    return;
  }

  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints == nullptr) {
    found = nullptr;
  }
  else {
    found = strstr(FailurePoints, checkValue);
  }

  if (found == nullptr) {
    // not yet found. so add it
    char* copy;
    size_t n;

    LOG_WARNING("activating intentional failure point '%s'. the server will misbehave!", value);
    n = strlen(checkValue);

    if (FailurePoints == nullptr) {
      copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, n + 1, false));

      if (copy == nullptr) {
        TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
        TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
        return;
      }

      memcpy(copy, checkValue, n);
      copy[n] = '\0';
    }
    else {
      copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, n + strlen(FailurePoints), false));

      if (copy == nullptr) {
        TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
        TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
        return;
      }

      memcpy(copy, FailurePoints, strlen(FailurePoints));
      memcpy(copy + strlen(FailurePoints) - 1, checkValue, n);
      copy[strlen(FailurePoints) + n - 1] = '\0';
    }

    FailurePoints = copy;
  }

  TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
  TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveFailurePointDebugging (char const* value) {
  char* checkValue;

  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints == nullptr) {
    TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
    return;
  }

  checkValue = MakeValue(value);

  if (checkValue != nullptr) {
    char* found;
    char* copy;
    size_t n;

    found = strstr(FailurePoints, checkValue);

    if (found == nullptr) {
      TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    if (strlen(FailurePoints) - strlen(checkValue) <= 2) {
      TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
      FailurePoints = nullptr;

      TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(FailurePoints) - strlen(checkValue) + 2, false));

    if (copy == nullptr) {
      TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    // copy start of string
    n = found - FailurePoints;
    memcpy(copy, FailurePoints, n);

    // copy remainder of string
    memcpy(copy + n, found + strlen(checkValue) - 1, strlen(FailurePoints) - strlen(checkValue) - n + 1);

    copy[strlen(FailurePoints) - strlen(checkValue) + 1] = '\0';
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
    FailurePoints = copy;

    TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
    TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearFailurePointsDebugging () {
  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = nullptr;

  TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseDebugging () {
  FailurePoints = nullptr;
  TRI_InitReadWriteLock(&FailurePointsLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownDebugging () {
  if (FailurePoints != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = nullptr;

  TRI_DestroyReadWriteLock(&FailurePointsLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
