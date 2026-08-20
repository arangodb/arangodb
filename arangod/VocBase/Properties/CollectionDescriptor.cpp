////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "CollectionDescriptor.h"

#include "Basics/Exceptions.h"
#include "Inspection/VPack.h"

#include <absl/strings/str_cat.h>

namespace arangodb {

CollectionDescriptor CollectionDescriptor::fromVelocyPack(
    velocypack::Slice info) {
  CollectionDescriptor props;
  auto status = velocypack::deserializeWithStatus(
      info, props, {.ignoreUnknownFields = true}, InspectInternalContext{});
  if (!status.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("failed to parse collection properties: ",
                     status.error()));
  }
  return props;
}

auto CollectionDescriptor::Invariants::isSmartConfiguration(
    CollectionDescriptor const& d) -> inspection::Status {
  auto const& shardKeys = d.clusteringConstant.shardKeys;
  if (d.internal.smartGraphAttribute.has_value()) {
    if (d.constant.getType() != TRI_COL_TYPE_DOCUMENT) {
      return {"Only document collections can have a smartGraphAttribute."};
    }
    if (!d.constant.isSmart) {
      return {
          "A smart vertex collection needs to be "
          "marked with \"isSmart: true\"."};
    }
    if (!shardKeys.has_value() || shardKeys->size() != 1 ||
        shardKeys->at(0) != StaticStrings::PrefixOfKeyString) {
      return {
          R"(A smart vertex collection needs to have "shardKeys": ["_key:"].)"};
    }
  } else if (d.constant.isSmart) {
    if (d.constant.getType() == TRI_COL_TYPE_EDGE) {
      if (shardKeys.has_value()) {
        // Check if SmartSharding is set correctly, but only if we have one.
        // Otherwise our default sharding will set correct values.
        if (shardKeys->size() != 1) {
          return {R"(A smart collection needs to have a single shardKey)"};
        }
        if (shardKeys->at(0) != StaticStrings::PrefixOfKeyString &&
            shardKeys->at(0) != StaticStrings::PostfixOfKeyString &&
            shardKeys->at(0) != StaticStrings::KeyString) {
          // For Smart Edges Post and Prefix are allowed (for connecting
          // satellites). Also just _key is allowed, as the shardKey for this
          // collection is not really used. We use the shadows ShardKeys,
          // which are _key based.
          return {
              R"(A smart edge collection needs to have "shardKeys": ["_key:"], [":_key"] or ["_key"].)"};
        }
      }
    } else {
      if (!shardKeys.has_value() || shardKeys->size() != 1 ||
          shardKeys->at(0) != StaticStrings::PrefixOfKeyString) {
        return {R"(A smart collection needs to have "shardKeys": ["_key:"].)"};
      }
    }
  }
  return inspection::Status::Success{};
}

}  // namespace arangodb