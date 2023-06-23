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
#include "Inspection/Access.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/KeyGeneratorProperties.h"
#include "VocBase/Properties/UtilityInvariants.h"
#include "VocBase/voc-types.h"

#include <optional>
#include <string>
#include <vector>

namespace arangodb {
/**
 * @brief All Properties of a Collection that are set by the user, but cannot be
 * modified after the Collection is created.
 */
struct CollectionConstantProperties {
  struct Invariants {
    [[nodiscard]] static auto isSmartConfiguration(
        CollectionConstantProperties const& props) -> inspection::Status;
  };

  std::underlying_type_t<TRI_col_type_e> type =
      TRI_col_type_e::TRI_COL_TYPE_DOCUMENT;
  bool isSystem = false;

  inspection::NonNullOptional<std::string> smartJoinAttribute = std::nullopt;

  KeyGeneratorProperties keyOptions{};

  // NOTE: These attributes are not documented
  bool isSmart = false;
  bool isDisjoint = false;
  bool cacheEnabled = false;

  inspection::NonNullOptional<std::string> smartGraphAttribute = std::nullopt;
  inspection::NonNullOptional<std::vector<DataSourceId>> shadowCollections =
      std::nullopt;

  // TODO: Maybe this is better off with a transformator Uint -> col_type_e
  [[nodiscard]] TRI_col_type_e getType() const noexcept {
    return TRI_col_type_e(type);
  }

  bool operator==(CollectionConstantProperties const&) const;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionConstantProperties& props) {
  auto shadowCollectionsField = std::invoke([&]() {
    if constexpr (!Inspector::isLoading) {
      // Write out the shadowCollections
      return f.field(StaticStrings::ShadowCollections, props.shadowCollections);
    } else {
      // Ignore the shadowCollections on input, this is not a user-modifyable
      // value
      return f.ignoreField(StaticStrings::ShadowCollections);
    }
  });

  return f.object(props)
      .fields(
          f.field(StaticStrings::DataSourceSystem, props.isSystem)
              .fallback(f.keep()),
          f.field(StaticStrings::IsSmart, props.isSmart).fallback(f.keep()),
          f.field(StaticStrings::IsDisjoint, props.isDisjoint)
              .fallback(f.keep()),
          f.field(StaticStrings::CacheEnabled, props.cacheEnabled)
              .fallback(f.keep()),
          f.field(StaticStrings::GraphSmartGraphAttribute,
                  props.smartGraphAttribute)
              .invariant(UtilityInvariants::isNonEmptyIfPresent),
          f.field(StaticStrings::SmartJoinAttribute, props.smartJoinAttribute)
              .invariant(UtilityInvariants::isNonEmptyIfPresent),
          f.field(StaticStrings::DataSourceType, props.type)
              .fallback(f.keep())
              .invariant(UtilityInvariants::isValidCollectionType),
          f.field(StaticStrings::KeyOptions, props.keyOptions)
              .fallback(f.keep()),
          /* Backwards compatibility, fields are allowed (MMFILES) but have no
             relevance anymore */
          f.ignoreField("doCompact"), f.ignoreField("isVolatile"),
          std::move(shadowCollectionsField))
      .invariant(
          CollectionConstantProperties::Invariants::isSmartConfiguration);
}

}  // namespace arangodb
