////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase database defaults
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

#ifndef TRIAGENS_VOC_BASE_VOCBASE_DEFAULTS_H
#define TRIAGENS_VOC_BASE_VOCBASE_DEFAULTS_H 1

#include "BasicsC/common.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TRI_json_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default settings
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vocbase_defaults_s {
  TRI_voc_size_t    defaultMaximalSize;
  bool              removeOnDrop;
  bool              defaultWaitForSync;
  bool              forceSyncProperties;
  bool              requireAuthentication;
  bool              requireAuthenticationUnixSockets;
  bool              authenticateSystemOnly;
}
TRI_vocbase_defaults_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief apply default settings
////////////////////////////////////////////////////////////////////////////////

void TRI_ApplyVocBaseDefaults (struct TRI_vocbase_s*,
                               TRI_vocbase_defaults_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert defaults into a JSON array
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonVocBaseDefaults (TRI_memory_zone_t*,
                                            TRI_vocbase_defaults_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief enhance defaults with data from JSON
////////////////////////////////////////////////////////////////////////////////

void TRI_FromJsonVocBaseDefaults (TRI_vocbase_defaults_t*,
                                  struct TRI_json_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
