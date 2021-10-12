////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Transaction/Hints.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class Result;

namespace transaction {
class Methods;
}

class ITransactionable {
 public:
  virtual ~ITransactionable() = default;

  /// @brief begin a transaction
  [[nodiscard]] virtual arangodb::Result beginTransaction(transaction::Hints hints) = 0;

  /// @brief commit a transaction
  [[nodiscard]] virtual arangodb::Result commitTransaction(transaction::Methods* trx) = 0;

  /// @brief abort a transaction
  [[nodiscard]] virtual arangodb::Result abortTransaction(transaction::Methods* trx) = 0;

  /// @brief return number of commits.
  /// for cluster transactions on coordinator, this either returns 0 or 1.
  /// for leader, follower or single-server transactions, this can include any
  /// number, because it will also include intermediate commits.
  [[nodiscard]] virtual uint64_t numCommits() const = 0;

  [[nodiscard]] virtual bool hasFailedOperations() const = 0;

  [[nodiscard]] virtual TRI_voc_tick_t lastOperationTick() const noexcept = 0;

  [[nodiscard]] virtual transaction::Status status() const noexcept = 0;
  [[nodiscard]] virtual bool isReadOnlyTransaction() const noexcept = 0;
  [[nodiscard]] virtual bool isWriteOrExclusiveTransaction() const noexcept = 0;
  virtual void setType(AccessMode::Type type) noexcept = 0;
  virtual void upgradeTypeIfNecessary(AccessMode::Type type) noexcept = 0;

  virtual void beginQuery(bool isModificationQuery) = 0;
  virtual void endQuery(bool isModificationQuery) noexcept = 0;
};

class Transactionable : public ITransactionable {
 public:
  virtual ~Transactionable();

  [[nodiscard]] transaction::Status status() const noexcept final {
    return _status;
  }

  [[nodiscard]] bool isReadOnlyTransaction() const noexcept final {
    return _type == AccessMode::Type::READ;
  }
  [[nodiscard]] bool isWriteOrExclusiveTransaction() const noexcept final {
    return _type > AccessMode::Type::READ;
  }
  void setType(AccessMode::Type type) noexcept final {
    _type = type;
  }
  virtual void upgradeTypeIfNecessary(AccessMode::Type type) noexcept final {
    setType(std::max(_type, type));
  }

 protected:
  void updateStatus(transaction::Status status) noexcept;

 private:
  transaction::Status _status = transaction::Status::CREATED;
  /// @brief access type (read|write)
  AccessMode::Type _type = AccessMode::Type::READ;
};

}  // namespace arangodb
