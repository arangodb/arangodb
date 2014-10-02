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

#ifndef ARANGODB_BASICS_C_DEBUGGING_H
#define ARANGODB_BASICS_C_DEBUGGING_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief macro TRI_IF_FAILURE
/// this macro can be used in maintainer mode to make the server fail at
/// certain locations in the C code. The points at which a failure is actually
/// triggered can be defined at runtime using TRI_AddFailurePointDebugging().
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS

#define TRI_IF_FAILURE(what) if (TRI_ShouldFailDebugging(what))

#else

#define TRI_IF_FAILURE(what) if (false)

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cause a segmentation violation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS
void TRI_SegfaultDebugging (char const*);
#else
static inline void TRI_SegfaultDebugging (char const* unused) {
  (void) unused;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we should fail at a failure point
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS
bool TRI_ShouldFailDebugging (char const*);
#else
static inline bool TRI_ShouldFailDebugging (char const* unused) {
  (void) unused;
  return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief add a failure point
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS
void TRI_AddFailurePointDebugging (char const*);
#else
static inline void TRI_AddFailurePointDebugging (char const* unused) {
  (void) unused;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a failure point
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS
void TRI_RemoveFailurePointDebugging (char const*);
#else
static inline void TRI_RemoveFailurePointDebugging (char const* unused) {
  (void) unused;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all failure points
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FAILURE_TESTS
void TRI_ClearFailurePointsDebugging (void);
#else
static inline void TRI_ClearFailurePointsDebugging (void) {
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether failure point debugging can be used
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_CanUseFailurePointsDebugging (void) {
#ifdef TRI_ENABLE_FAILURE_TESTS
  return true;
#else
  return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseDebugging (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownDebugging (void);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
