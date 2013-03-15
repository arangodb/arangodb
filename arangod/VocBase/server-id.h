////////////////////////////////////////////////////////////////////////////////
/// @brief server id handling
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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_SERVER_ID_H
#define TRIAGENS_VOC_BASE_SERVER_ID_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief mask value for significant bits of server id
////////////////////////////////////////////////////////////////////////////////

#define TRI_SERVER_ID_MASK 0x0000FFFFFFFFFFFFULL

////////////////////////////////////////////////////////////////////////////////
/// @brief server id type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_server_id_t;

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
/// @brief initialise the global server id to 0 on startup
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseServerId (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetServerId (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief establish the global server id
////////////////////////////////////////////////////////////////////////////////

void TRI_EstablishServerId (const TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief reads server id from file
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadServerId (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes server id to file
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteServerId (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new server id
////////////////////////////////////////////////////////////////////////////////

int TRI_GenerateServerId (void);

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
