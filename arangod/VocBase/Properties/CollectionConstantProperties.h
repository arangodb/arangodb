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
#include "Inspection/Types.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/KeyGeneratorProperties.h"
#include "VocBase/Properties/InspectContexts.h"
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
  std::underlying_type_t<TRI_col_type_e> type =
      TRI_col_type_e::TRI_COL_TYPE_DOCUMENT;
  bool isSystem = false;

  inspection::NonNullOptional<std::string> smartJoinAttribute = std::nullopt;

  KeyGeneratorProperties keyOptions{};

  // NOTE: These attributes are not documented
  bool isSmart = false;
  bool isDisjoint = false;

  inspection::NonNullOptional<std::vector<DataSourceId>> shadowCollections =
      std::nullopt;

  // TODO: Maybe this is better off with a transformator Uint -> col_type_e
  [[nodiscard]] TRI_col_type_e getType() const noexcept {
    return TRI_col_type_e(type);
  }

  bool operator==(CollectionConstantProperties const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionConstantProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::DataSourceSystem, props.isSystem)
          .fallback(f.keep()),
      f.field(StaticStrings::IsSmart, props.isSmart).fallback(f.keep()),
      f.field(StaticStrings::IsDisjoint, props.isDisjoint).fallback(f.keep()),
      userInvariant(
          f,
          f.field(StaticStrings::SmartJoinAttribute, props.smartJoinAttribute),
          UtilityInvariants::isNonEmptyIfPresent),
      userInvariant(
          f,
          f.field(StaticStrings::DataSourceType, props.type).fallback(f.keep()),
          UtilityInvariants::isValidCollectionType),
      f.field(StaticStrings::KeyOptions, props.keyOptions).fallback(f.keep()),
      /* Backwards compatibility, fields are allowed (MMFILES) but have no
         relevance anymore */
      f.ignoreField("doCompact"), f.ignoreField("isVolatile"),
      // Written by the server, so always written out. A coordinator loading a
      // plan entry needs these to resolve the existing shadow collections
      // instead of building new ones, which would come out with the parent's
      // numberOfShards of 0. User input is accepted and dropped.
      f.field(StaticStrings::ShadowCollections, props.shadowCollections)
          .fallback(f.keep())
          .whenLoading([]() {
            return isInternalContext<Inspector> || isAgencyContext<Inspector>
                       ? inspection::FieldCondition::Process
                       : inspection::FieldCondition::Ignore;
          }));
}

}  // namespace arangodb
