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

#ifndef ARANGOD_MMFILES_MMFILES_REPLICATION_COMMON_H
#define ARANGOD_MMFILES_MMFILES_REPLICATION_COMMON_H 1

#include "MMFiles/MMFilesDatafile.h"
#include "Replication/common-defines.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace mmfilesutils {
/// @brief whether or not a marker should be replicated
bool MustReplicateWalMarkerType(MMFilesMarker const* marker, bool allowDBMarkers);

/// @brief whether or not a marker belongs to a transaction
bool IsTransactionWalMarkerType(MMFilesMarker const* marker);

/// @brief translate a marker type to a replication type
TRI_replication_operation_e TranslateType(MMFilesMarker const* marker);
}  // namespace mmfilesutils
}  // namespace arangodb

#endif
