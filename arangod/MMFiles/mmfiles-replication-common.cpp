////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "mmfiles-replication-common.h"

using namespace arangodb::mmfilesutils;
namespace arangodb {
namespace mmfilesutils {
  
/// @brief whether or not a marker should be replicated
bool MustReplicateWalMarkerType(MMFilesMarker const* marker, bool allowDBMarkers) {
  MMFilesMarkerType type = marker->getType();
  return (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
          type == TRI_DF_MARKER_VPACK_REMOVE ||
          type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
          type == TRI_DF_MARKER_VPACK_DROP_INDEX ||
          (allowDBMarkers && (type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
                              type == TRI_DF_MARKER_VPACK_DROP_DATABASE)) ||
          type == TRI_DF_MARKER_VPACK_CREATE_VIEW ||
          type == TRI_DF_MARKER_VPACK_DROP_VIEW ||
          type == TRI_DF_MARKER_VPACK_CHANGE_VIEW);
}

/// @brief whether or not a marker belongs to a transaction
bool IsTransactionWalMarkerType(MMFilesMarker const* marker) {
  MMFilesMarkerType type = marker->getType();
  return (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION);
}

/// @brief translate a marker type to a replication type
TRI_replication_operation_e TranslateType(MMFilesMarker const* marker) {
  switch (marker->getType()) {
    case TRI_DF_MARKER_VPACK_DOCUMENT:
      return REPLICATION_MARKER_DOCUMENT;
    case TRI_DF_MARKER_VPACK_REMOVE:
      return REPLICATION_MARKER_REMOVE;
    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
      return REPLICATION_TRANSACTION_START;
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
      return REPLICATION_TRANSACTION_COMMIT;
    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION:
      return REPLICATION_TRANSACTION_ABORT;
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
      return REPLICATION_COLLECTION_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
      return REPLICATION_COLLECTION_DROP;
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
      return REPLICATION_COLLECTION_RENAME;
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
      return REPLICATION_COLLECTION_CHANGE;
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
      return REPLICATION_INDEX_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_INDEX:
      return REPLICATION_INDEX_DROP;
    case TRI_DF_MARKER_VPACK_CREATE_DATABASE:
      return REPLICATION_DATABASE_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_DATABASE:
      return REPLICATION_DATABASE_DROP;
    case TRI_DF_MARKER_VPACK_CREATE_VIEW:
      return REPLICATION_VIEW_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_VIEW:
      return REPLICATION_VIEW_DROP;
    case TRI_DF_MARKER_VPACK_CHANGE_VIEW:
      return REPLICATION_VIEW_CHANGE;
      
    default:
      TRI_ASSERT(false);
      return REPLICATION_INVALID;
  }
}

}
}
