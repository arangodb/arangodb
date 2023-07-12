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
#include "Inspection/Access.h"

#include <velocypack/Builder.h>

namespace arangodb {

struct CollectionMutableProperties {

  struct Invariants {
    [[nodiscard]] static auto isJsonSchema(
        inspection::NonNullOptional<arangodb::velocypack::Builder> const& value) -> inspection::Status;
  };
  std::string name = StaticStrings::Empty;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder computedValues{VPackSlice::nullSlice()};

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  inspection::NonNullOptional<arangodb::velocypack::Builder> schema{std::nullopt};

  bool operator==(CollectionMutableProperties const&) const;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionMutableProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::DataSourceName, props.name)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isNonEmpty),
      f.field(StaticStrings::Schema, props.schema)
          .fallback(f.keep())
          .invariant(CollectionMutableProperties::Invariants::isJsonSchema),
      f.field(StaticStrings::ComputedValues, props.computedValues)
          .fallback(f.keep()));
}

}  // namespace arangodb
