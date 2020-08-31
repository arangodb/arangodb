////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Identifiers/LocalDocumentId.h"

#include "VocBase/Identifiers/RevisionId.h"

namespace arangodb {

LocalDocumentId::LocalDocumentId(RevisionId const& id) noexcept
    : Identifier(id.id()) {}

bool LocalDocumentId::isSet() const noexcept { return id() != 0; }

bool LocalDocumentId::empty() const noexcept { return !isSet(); }

/// @brief create a new document id
LocalDocumentId LocalDocumentId::create() {
  return LocalDocumentId(TRI_HybridLogicalClock());
}

/// @brief create a document id from an existing revision id
LocalDocumentId LocalDocumentId::create(RevisionId const& rid) {
  return LocalDocumentId{rid.id()};
}

void LocalDocumentId::track(LocalDocumentId const& id) {
  TRI_HybridLogicalClock(id.id());
}

}  // namespace arangodb
