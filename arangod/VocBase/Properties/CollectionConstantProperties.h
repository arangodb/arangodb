////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/StaticStrings.h"
#include "VocBase/Properties/UtilityInvariants.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <optional>
#include <numeric>
#include <string>
#include <vector>

namespace arangodb {
/**
 * @brief All Properties of a Collection that are set by the user, but cannot be
 * modified after the Collection is created.
 */
struct CollectionConstantProperties {
  std::underlying_type_t<TRI_col_type_e> type =
      TRI_col_type_e::TRI_COL_TYPE_DOCUMENT;
  bool isSystem = false;

  uint64_t numberOfShards = 1;

  std::string distributeShardsLike = StaticStrings::Empty;
  std::string shardingStrategy = StaticStrings::Empty;
  std::optional<std::string> smartJoinAttribute = std::nullopt;

  std::vector<std::string> shardKeys;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder keyOptions =
      VPackBuilder{VPackSlice::emptyObjectSlice()};

  // NOTE: These attributes are not documented
  bool isSmart = false;
  bool isDisjoint = false;
  bool doCompact = false;
  bool isVolatile = false;
  bool cacheEnabled = false;

  std::string smartGraphAttribute = StaticStrings::Empty;

  // TODO: Maybe this is better off with a transformator Uint -> col_type_e
  [[nodiscard]] TRI_col_type_e getType() const noexcept {
    return TRI_col_type_e(type);
  }
};

template<class Inspector>
auto inspect(Inspector& f, CollectionConstantProperties& props) {
  return f.object(props).fields(
      f.field("isSystem", props.isSystem).fallback(f.keep()),
      f.field("isSmart", props.isSmart).fallback(f.keep()),
      f.field("isDisjoint", props.isDisjoint).fallback(f.keep()),
      f.field("doCompact", props.doCompact).fallback(f.keep()),
      f.field("cacheEnabled", props.cacheEnabled).fallback(f.keep()),
      f.field("isVolatile", props.isVolatile).fallback(f.keep()),
      f.field("smartGraphAttribute", props.smartGraphAttribute)
          .fallback(f.keep()),
      f.field("numberOfShards", props.numberOfShards)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isGreaterZero),
      f.field("distributeShardsLike", props.distributeShardsLike)
          .fallback(f.keep()),
      f.field(StaticStrings::SmartJoinAttribute, props.smartJoinAttribute)
          .invariant(UtilityInvariants::isNonEmptyIfPresent),
      f.field("shardingStrategy", props.shardingStrategy)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isValidShardingStrategy),
      f.field("shardKeys", props.shardKeys)
          .fallback(std::vector<std::string>{StaticStrings::KeyString})
          .invariant(UtilityInvariants::areShardKeysValid),
      f.field("type", props.type)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isValidCollectionType),
      f.field("keyOptions", props.keyOptions).fallback(f.keep()));
}

}  // namespace arangodb
