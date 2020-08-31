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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "StandaloneContext.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
  
StandaloneContext::StandaloneContext(TRI_vocbase_t& vocbase)
  : SmartContext(vocbase, Context::makeTransactionId(), nullptr) {}

/// @brief get transaction state, determine commit responsibility
/*virtual*/ std::shared_ptr<TransactionState> StandaloneContext::acquireState(transaction::Options const& options,
                                                                              bool& responsibleForCommit) {
  if (_state) {
    responsibleForCommit = false;
  } else {
    responsibleForCommit = true;
    _state = transaction::Context::createState(options);
  }
  return _state;
}

/// @brief unregister the transaction
void StandaloneContext::unregisterTransaction() noexcept {
  TRI_ASSERT(_state != nullptr);
  _state = nullptr;
}

std::shared_ptr<Context> StandaloneContext::clone() const {
  auto clone = std::make_shared<transaction::StandaloneContext>(_vocbase);
  clone->setState(_state);
  return clone;
}

/// @brief create a context, returned in a shared ptr
/*static*/ std::shared_ptr<transaction::Context> transaction::StandaloneContext::Create(TRI_vocbase_t& vocbase) {
  return std::make_shared<transaction::StandaloneContext>(vocbase);
}

}  // namespace transaction
}  // namespace arangodb
