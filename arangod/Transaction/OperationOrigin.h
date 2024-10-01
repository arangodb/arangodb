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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string_view>

namespace arangodb::transaction {

// simple helper struct that indicates the origin of an operation.
// we use this to categorize operations for testing memory usage tracking.
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

  // must remain valid during the entire lifetime of the OperationOrigin
  std::string_view const description;
  Type const type;

 protected:
  // note: the caller is responsible for ensuring that description
  // stays valid for the entire lifetime of the object
  constexpr OperationOrigin(std::string_view description, Type type) noexcept
      : description(description), type(type) {}
};

// an operation that is an AQL query.
// note: the caller is responsible for ensuring that description
// stays valid for the entire lifetime of the object
struct OperationOriginAQL : OperationOrigin {
  constexpr OperationOriginAQL(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kAQL) {}
};

// an operation that is an end user-initiated operation, but not AQL.
// note: the caller is responsible for ensuring that description
// stays valid for the entire lifetime of the object
struct OperationOriginREST : OperationOrigin {
  constexpr OperationOriginREST(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kREST) {}
};

// an internal operation, not directly initiated by end users.
// note: the caller is responsible for ensuring that description
// stays valid for the entire lifetime of the object
struct OperationOriginInternal : OperationOrigin {
  constexpr OperationOriginInternal(std::string_view description) noexcept
      : OperationOrigin(description, OperationOrigin::Type::kInternal) {}
};

#ifdef ARANGODB_USE_GOOGLE_TESTS
// an operation from inside a test case. we count them as internal, too.
struct OperationOriginTestCase : OperationOrigin {
  constexpr OperationOriginTestCase() noexcept
      : OperationOrigin("unit test", OperationOrigin::Type::kInternal) {}
};
#endif

}  // namespace arangodb::transaction
