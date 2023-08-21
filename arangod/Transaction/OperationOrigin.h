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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string_view>

namespace arangodb::transaction {

struct OperationOrigin {
  // type of transaction
  enum class Type : std::uint8_t {
    // initiated by user via top-level AQL query
    kAQL = 0,
    // initiated by user via REST call/JavaScript console/Foxx action
    kREST = 1,
    // internal operation (statistics, TTL index removals, etc.)
    kInternal = 2,
  };

  OperationOrigin(OperationOrigin const&) = default;
  OperationOrigin(OperationOrigin&&) = default;
  OperationOrigin& operator=(OperationOrigin const&) = delete;
  OperationOrigin& operator=(OperationOrigin&&) = delete;

  std::string_view const description;
  Type const type;

 protected:
  // note: the caller is responsible for ensuring that description
  // stays valid for the entire lifetime of the object
  constexpr OperationOrigin(std::string_view description, Type type) noexcept
      : description(description), type(type) {}
};

struct OperationOriginAQL : OperationOrigin {
  constexpr OperationOriginAQL(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kAQL) {}
};

struct OperationOriginREST : OperationOrigin {
  constexpr OperationOriginREST(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kREST) {}
};

struct OperationOriginInternal : OperationOrigin {
  constexpr OperationOriginInternal(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kInternal) {}
};

#ifdef ARANGODB_USE_GOOGLE_TESTS
struct OperationOriginTestCase : OperationOrigin {
  constexpr OperationOriginTestCase() noexcept
      : OperationOrigin("unit test", OperationOrigin::Type::kInternal) {}
};
#endif

}  // namespace arangodb::transaction
