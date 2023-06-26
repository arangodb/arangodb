////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iosfwd>
#include <limits>
#include <memory>
#include <string>

#include <velocypack/Builder.h>

#include "Inspection/Factory.h"

namespace arangodb {
class RebootId {
 public:
  using value_type = uint64_t;

  explicit constexpr RebootId() noexcept = delete;
  explicit constexpr RebootId(value_type rebootId) noexcept
      : _value(rebootId) {}
  [[nodiscard]] value_type value() const noexcept { return _value; }

  [[nodiscard]] bool initialized() const noexcept { return value() != 0; }

  [[nodiscard]] bool operator==(RebootId other) const noexcept {
    return value() == other.value();
  }
  [[nodiscard]] bool operator!=(RebootId other) const noexcept {
    return value() != other.value();
  }
  [[nodiscard]] bool operator<(RebootId other) const noexcept {
    return value() < other.value();
  }
  [[nodiscard]] bool operator>(RebootId other) const noexcept {
    return value() > other.value();
  }
  [[nodiscard]] bool operator<=(RebootId other) const noexcept {
    return value() <= other.value();
  }
  [[nodiscard]] bool operator>=(RebootId other) const noexcept {
    return value() >= other.value();
  }

  [[nodiscard]] static constexpr RebootId max() noexcept {
    return RebootId{std::numeric_limits<value_type>::max()};
  }

  std::ostream& print(std::ostream& o) const;

 private:
  value_type _value{};
};

template<class Inspector>
auto inspect(Inspector& f, RebootId& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = RebootId{v};
    }
    return res;
  } else {
    auto v = x.value();
    return f.apply(v);
  }
}

std::ostream& operator<<(std::ostream& o, arangodb::RebootId const& r);

template<>
struct velocypack::Extractor<arangodb::RebootId> {
  static auto extract(velocypack::Slice slice) -> RebootId {
    return RebootId{slice.getNumericValue<std::size_t>()};
  }
};

namespace inspection {
template<>
struct Factory<RebootId> : BaseFactory<RebootId> {
  static auto make_value() -> RebootId { return RebootId(0); }
};
}  // namespace inspection

}  // namespace arangodb
