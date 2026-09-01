////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/StaticStrings.h"
#include "Inspection/Access.h"
#include "VocBase/Properties/InspectContexts.h"
#include "VocBase/Properties/UtilityInvariants.h"

#include <cstdint>
#include <string>

namespace arangodb {

struct DatabaseConfiguration;
class Result;

namespace inspection {
struct Status;
}

struct CollectionInternalProperties {
  // Set when the collection is dropped, read back on load.
  bool deleted = false;

  bool syncByRevision = true;
  bool usesRevisionsAsDocumentIds = true;
  bool isSmartChild = false;
  uint64_t internalValidatorType = 0;

  // The user supplies this when creating a SmartGraph, but the DBServer
  // maintenance task also fills it in during an upgrade, so it changes at
  // runtime and cannot live with the immutable properties.
  inspection::NonNullOptional<std::string> smartGraphAttribute = std::nullopt;

  bool operator==(CollectionInternalProperties const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionInternalProperties& props) {
  return f.object(props).fields(
      internalOnlyField(f, StaticStrings::DataSourceDeleted, props.deleted),
      f.field(StaticStrings::SyncByRevision, props.syncByRevision)
          .fallback(f.keep()),
      f.field(StaticStrings::UsesRevisionsAsDocumentIds,
              props.usesRevisionsAsDocumentIds)
          .fallback(f.keep()),
      f.field(StaticStrings::IsSmartChild, props.isSmartChild)
          .fallback(f.keep()),
      f.field(StaticStrings::InternalValidatorTypes,
              props.internalValidatorType)
          .fallback(f.keep()),
      userInvariant(f,
                    f.field(StaticStrings::GraphSmartGraphAttribute,
                            props.smartGraphAttribute),
                    UtilityInvariants::isNonEmptyIfPresent));
}

}  // namespace arangodb
