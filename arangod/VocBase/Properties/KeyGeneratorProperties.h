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
#include "Inspection/Types.h"

#include <variant>

namespace arangodb {

namespace inspection {
struct Status;
}  // namespace inspection

struct TraditionalKeyGeneratorProperties {
  bool allowUserKeys{true};
  // TODO: This is not allowed in Cluster, only SingleServer, for now it is
  // ignored on Generator creation
  uint64_t lastValue{0};

  bool operator==(TraditionalKeyGeneratorProperties const&) const noexcept =
      default;
};

template<class Inspector>
auto inspect(Inspector& f, TraditionalKeyGeneratorProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::AllowUserKeys, props.allowUserKeys)
          .fallback(f.keep()),
      f.field(StaticStrings::LastValue, props.lastValue).fallback(f.keep()));
}

struct AutoIncrementGeneratorProperties {
  struct Invariants {
    [[nodiscard]] static auto isReasonableOffsetValue(uint64_t const& offset)
        -> inspection::Status;
    [[nodiscard]] static auto isReasonableIncrementValue(
        uint64_t const& increment) -> inspection::Status;
  };

  bool allowUserKeys{true};
  uint64_t offset{0};
  uint64_t increment{1};

  // TODO: This is not allowed in Cluster, only SingleServer, for now it is
  // ignored on Generator creation
  uint64_t lastValue{0};

  bool operator==(AutoIncrementGeneratorProperties const&) const noexcept =
      default;
};

template<class Inspector>
auto inspect(Inspector& f, AutoIncrementGeneratorProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::AllowUserKeys, props.allowUserKeys)
          .fallback(f.keep()),
      f.field("increment", props.increment)
          .fallback(f.keep())
          .invariant(AutoIncrementGeneratorProperties::Invariants::
                         isReasonableIncrementValue),
      f.field("offset", props.offset)
          .fallback(f.keep())
          .invariant(AutoIncrementGeneratorProperties::Invariants::
                         isReasonableOffsetValue),
      f.field(StaticStrings::LastValue, props.lastValue).fallback(f.keep()));
}

struct UUIDKeyGeneratorProperties {
  bool allowUserKeys{true};

  bool operator==(UUIDKeyGeneratorProperties const&) const noexcept = default;
};

template<class Inspector>
auto inspect(Inspector& f, UUIDKeyGeneratorProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::AllowUserKeys, props.allowUserKeys)
          .fallback(f.keep()));
}

struct PaddedKeyGeneratorProperties {
  bool allowUserKeys{true};
  // TODO: This is the only one that IS ALLOWED in Cluster.
  uint64_t lastValue{0};

  bool operator==(PaddedKeyGeneratorProperties const&) const noexcept = default;
};

template<class Inspector>
auto inspect(Inspector& f, PaddedKeyGeneratorProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::AllowUserKeys, props.allowUserKeys)
          .fallback(f.keep()),
      f.field(StaticStrings::LastValue, props.lastValue).fallback(f.keep()));
}

using KeyGeneratorProperties =
    std::variant<TraditionalKeyGeneratorProperties,
                 AutoIncrementGeneratorProperties, UUIDKeyGeneratorProperties,
                 PaddedKeyGeneratorProperties>;

template<class Inspector>
auto inspect(Inspector& f, KeyGeneratorProperties& props) {
  if constexpr (Inspector::isLoading) {
    // Special handling if user omits type name, we fall back
    // to traditional.
    if (f.slice().isObject() && !f.slice().hasKey("type")) {
      TraditionalKeyGeneratorProperties tmp;
      auto status = f.apply(tmp);
      if (status.ok()) {
        props = tmp;
      }
      return status;
    }
  }
  return f.variant(props).embedded("type").alternatives(
      inspection::type<TraditionalKeyGeneratorProperties>("traditional"),
      inspection::type<AutoIncrementGeneratorProperties>("autoincrement"),
      inspection::type<UUIDKeyGeneratorProperties>("uuid"),
      inspection::type<PaddedKeyGeneratorProperties>("padded"));
}

}  // namespace arangodb
