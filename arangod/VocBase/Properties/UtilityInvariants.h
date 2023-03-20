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

#include "VocBase/voc-types.h"

#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

namespace inspection {
struct Status;
}  // namespace inspection

struct UtilityInvariants {
  [[nodiscard]] static auto isNonEmpty(std::string const& value)
      -> inspection::Status;

  [[nodiscard]] static auto isNonEmptyIfPresent(
      std::optional<std::string> const& value) -> inspection::Status;

  [[nodiscard]] static auto isGreaterZero(uint64_t const& value)
      -> inspection::Status;
  [[nodiscard]] static auto isGreaterZeroIfPresent(
      std::optional<uint64_t> const& value) -> inspection::Status;

  [[nodiscard]] static auto isValidShardingStrategy(std::string const& value)
      -> inspection::Status;
  [[nodiscard]] static auto isValidShardingStrategyIfPresent(
      std::optional<std::string> const& value) -> inspection::Status;

  [[nodiscard]] static auto isValidCollectionType(
      std::underlying_type_t<TRI_col_type_e> const& value)
      -> inspection::Status;

 private:
  // Never instantiate. only static methods
  UtilityInvariants() = default;
};
}  // namespace arangodb
