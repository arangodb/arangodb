////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_V8_V8_VOCBASE_H
#define TRIAGENS_DURHAM_V8_V8_VOCBASE_H 1

#define TRI_WITHIN_C
#include <Basics/Common.h>
#include <VocBase/vocbase.h>
#undef TRI_WITHIN_C

#include <v8.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8VocBase V8-VocBase Bridge
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::Context> InitV8VocBridge (TRI_vocbase_t* vocbase);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
