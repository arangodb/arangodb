////////////////////////////////////////////////////////////////////////////////
/// @brief debugging helpers
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "BasicsC/debugging.h"
#include "BasicsC/locks.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Debugging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief a global string containing the currently registered failure points
/// the string is a comma-separated list of point names
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

static char* FailurePoints;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-write lock for thread-safe access to the failure-points list
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

TRI_read_write_lock_t FailurePointsLock;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Debugging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief make a delimited value from a string, so we can unambigiously 
/// search for it (e.g. searching for just "foo" would find "foo" and "foobar",
/// so we'll be putting the value inside some delimiter: ",foo,")
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

static char* MakeValue (char const* value) {
  char* delimited;

  if (value == NULL || strlen(value) == 0) {
    return NULL;
  }

  delimited = TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(value) + 3, false);

  if (delimited != NULL) {
    memcpy(delimited + 1, value, strlen(value));
    delimited[0] = ',';
    delimited[strlen(value) + 1] = ',';
    delimited[strlen(value) + 2] = '\0';
  }

  return delimited;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Debugging
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we should fail at a specific failure point
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

bool TRI_ShouldFailDebugging (char const* value) {
  char* found;
  char* checkValue;

  checkValue = MakeValue(value);

  if (checkValue == NULL) {
    return false;
  }

  TRI_ReadLockReadWriteLock(&FailurePointsLock);
  
  if (FailurePoints == NULL) {
    found = NULL;
  }
  else {
    found = strstr(FailurePoints, checkValue);
  }
  
  TRI_ReadUnlockReadWriteLock(&FailurePointsLock);

  TRI_Free(TRI_CORE_MEM_ZONE, checkValue);

  return (found != NULL);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief add a failure point
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFailurePointDebugging (char const* value) {

#ifdef TRI_ENABLE_MAINTAINER_MODE

  char* found;
  char* checkValue;

  checkValue = MakeValue(value);
  
  if (checkValue == NULL) {
    return;
  }
  
  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints == NULL) {
    found = NULL;
  }
  else {
    found = strstr(FailurePoints, checkValue);
  }

  if (found == NULL) {
    // not yet found. so add it
    char* copy;
    size_t n;

    n = strlen(checkValue);

    if (FailurePoints == NULL) {
      copy = TRI_Allocate(TRI_CORE_MEM_ZONE, n + 1, false);

      if (copy == NULL) {
        TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
        TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
        return;
      }

      memcpy(copy, checkValue, n);
      copy[n] = '\0';
    }
    else {
      copy = TRI_Allocate(TRI_CORE_MEM_ZONE, n + strlen(FailurePoints), false);
      
      if (copy == NULL) {
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

#else

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveFailurePointDebugging (char const* value) {

#ifdef TRI_ENABLE_MAINTAINER_MODE

  char* checkValue;

  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints == NULL) {
    TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
    return;
  }
  
  checkValue = MakeValue(value);
  
  if (checkValue != NULL) {
    char* found; 
    char* copy;
    size_t n;

    found = strstr(FailurePoints, checkValue);

    if (found == NULL) {
      TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    if (strlen(FailurePoints) - strlen(checkValue) <= 2) {
      TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
      FailurePoints = NULL;

      TRI_WriteUnlockReadWriteLock(&FailurePointsLock);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    copy = TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(FailurePoints) - strlen(checkValue) + 2, false);

    if (copy == NULL) {
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

#else

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearFailurePointsDebugging () {
#ifdef TRI_ENABLE_MAINTAINER_MODE
      
  TRI_WriteLockReadWriteLock(&FailurePointsLock);

  if (FailurePoints != NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = NULL;
  
  TRI_WriteUnlockReadWriteLock(&FailurePointsLock);

#else

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether failure point debugging can be used
////////////////////////////////////////////////////////////////////////////////

bool TRI_CanUseFailurePointsDebugging () {
#ifdef TRI_ENABLE_MAINTAINER_MODE

  return true;

#else

  return false;

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseDebugging () {
#ifdef TRI_ENABLE_MAINTAINER_MODE

  FailurePoints = NULL;
  TRI_InitReadWriteLock(&FailurePointsLock);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownDebugging () {
#ifdef TRI_ENABLE_MAINTAINER_MODE

  if (FailurePoints != NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = NULL;

  TRI_DestroyReadWriteLock(&FailurePointsLock);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
