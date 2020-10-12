////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_IDENTIFIERS_TRANSACTION_ID_H
#define ARANGOD_VOCBASE_IDENTIFIERS_TRANSACTION_ID_H 1

#include "Basics/Identifier.h"
#include "VocBase/ticks.h"

namespace arangodb {

/// @brief transaction identifier type
class TransactionId : public basics::Identifier {
 public:
  constexpr TransactionId() noexcept : Identifier() {}
  constexpr explicit TransactionId(BaseType id) noexcept : Identifier(id) {}

 public:
  /// @brief whether or not the id is set (not 0)
  bool isSet() const noexcept;

  /// @brief whether or not the identifier is unset (equal to 0)
  bool empty() const noexcept;

  bool isCoordinatorTransactionId() const;
  bool isFollowerTransactionId() const;
  bool isLeaderTransactionId() const;
  bool isChildTransactionId() const;
  bool isLegacyTransactionId() const;

  uint32_t serverId() const;

  /// @brief create a child transaction (coordinator -> leader; leader -> follower)
  TransactionId child();

 public:
  /// @brief create a not-set document id
  static constexpr TransactionId none() { return TransactionId(0); }

  /// @brief create a coordinator id
  static TransactionId createSingleServer();

  /// @brief create a coordinator id
  static TransactionId createCoordinator();

  /// @brief create a legacy id
  static TransactionId createLegacy();
};

// TransactionId should not be bigger than the BaseType
static_assert(sizeof(TransactionId) == sizeof(TransactionId::BaseType),
              "invalid size of TransactionId");
}  // namespace arangodb

DECLARE_HASH_FOR_IDENTIFIER(arangodb::TransactionId)
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::TransactionId)

#endif
