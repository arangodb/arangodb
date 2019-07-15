////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_RECOVERY_TRANSACTION_STATE
#define ARANGODB_RECOVERY_TRANSACTION_STATE 1

#include "Transaction/StandaloneContext.h"

namespace arangodb {

template<typename StateImpl>
class RecoveryTransactionContext final : public arangodb::transaction::StandaloneContext {
 public:
  RecoveryTransactionContext(TRI_vocbase_t& vocbase, TRI_voc_tick_t tick)
    : arangodb::transaction::StandaloneContext(vocbase),
      _tick(tick) {
  }

  void registerTransaction(arangodb::TransactionState* state) override {
    if (!state) {
      TRI_ASSERT(false);
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto& impl = dynamic_cast<StateImpl&>(*state);
#else
    auto& impl = static_cast<StateImpl&>(*state);
#endif

    impl.lastOperationTick(_tick);
  }

 private:
  TRI_voc_tick_t _tick;
}; // RecoveryTransactionContext

} // arangodb

#endif // ARANGODB_RECOVERY_TRANSACTION_STATE
