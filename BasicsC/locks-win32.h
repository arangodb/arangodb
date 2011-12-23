////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in win32
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

#ifndef TRIAGENS_PHILADELPHIA_BASICS_C_LOCKS_WIN32_H
#define TRIAGENS_PHILADELPHIA_BASICS_C_LOCKS_WIN32_H 1

#include <BasicsC/common.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex type
////////////////////////////////////////////////////////////////////////////////

#define TRI_mutex_t CRITICAL_SECTION

////////////////////////////////////////////////////////////////////////////////
/// @brief read-write-lock type
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_read_write_lock_s {
    HANDLE _writerEvent;
    HANDLE _readersEvent;

    int _readers;

    CRITICAL_SECTION _lockWriter;
    CRITICAL_SECTION _lockReaders;
}
TRI_read_write_lock_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
