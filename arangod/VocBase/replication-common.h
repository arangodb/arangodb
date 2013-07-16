////////////////////////////////////////////////////////////////////////////////
/// @brief replication functions
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

#ifndef TRIAGENS_VOC_BASE_REPLICATION_COMMON_H
#define TRIAGENS_VOC_BASE_REPLICATION_COMMON_H 1

#include "BasicsC/common.h"

#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION 
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "check for more data?"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_CHECKMORE "x-arango-replication-checkmore"

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "last found tick"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_LASTFOUND "x-arango-replication-lastfound"

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "replication active"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_ACTIVE    "x-arango-replication-active"

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replication operations
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  REPLICATION_INVALID    = 0,

  REPLICATION_STOP       = 1000,
  REPLICATION_START      = 1001,

  COLLECTION_CREATE      = 2000,
  COLLECTION_DROP        = 2001,
  COLLECTION_RENAME      = 2002,
  COLLECTION_CHANGE      = 2003,

  INDEX_CREATE           = 2100,
  INDEX_DROP             = 2101,

  TRI_TRANSACTION_START  = 2200,
  TRI_TRANSACTION_COMMIT = 2201,

  MARKER_DOCUMENT        = 2300,
  MARKER_EDGE            = 2301,
  MARKER_REMOVE          = 2302,

  REPLICATION_MAX
}
TRI_replication_operation_e;

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
/// @brief determine whether a collection should be included in replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExcludeCollectionReplication (const char*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
