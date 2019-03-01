////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "SmartContext.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ticks.h"

#include "Logger/Logger.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
  
SmartContext::SmartContext(TRI_vocbase_t& vocbase,
                           TRI_voc_tid_t globalId,
                           TransactionState* state)
  : Context(vocbase), _globalId(globalId), _state(state) {
  TRI_ASSERT(_globalId != 0);
}
  
SmartContext::~SmartContext() {
  if (_state) {
    if (_state->isTopLevelTransaction()) {
      TRI_ASSERT(false); // probably should not happen
      delete _state;
    } else {
      _state->decreaseNesting();
    }
  }
}
  
/// @brief order a custom type handler for the collection
std::shared_ptr<arangodb::velocypack::CustomTypeHandler> transaction::SmartContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler.reset(
        transaction::Context::createCustomTypeHandler(_vocbase, resolver()));
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler;
}

/// @brief return the resolver
CollectionNameResolver const& transaction::SmartContext::resolver() {
  if (_resolver == nullptr) {
    createResolver();
  }
  TRI_ASSERT(_resolver != nullptr);
  return *_resolver;
}

TRI_voc_tid_t transaction::SmartContext::generateId() const {
  TRI_ASSERT(!transaction::isLegacyTransactionId(_globalId));
  LOG_DEVEL << "Using mananged ID " << _globalId << " mod 4: " << (_globalId % 4);
  return _globalId;
}
  
//  ============= ManagedContext =============
  
ManagedContext::ManagedContext(TRI_voc_tid_t globalId,
                               TransactionState* state,
                               AccessMode::Type mode)
  : SmartContext(state->vocbase(), globalId, state), _mode(mode) {}
  
ManagedContext::~ManagedContext() {
  if (_state) {
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->returnManagedTrx(_globalId, _mode);
  }
}

/// @brief get parent transaction (if any) increase nesting
TransactionState* ManagedContext::leaseParentTransaction() {
  TRI_ASSERT(_state);
  // single document transaction should never be leased out
  TRI_ASSERT(!_state->hasHint(Hints::Hint::SINGLE_OPERATION));
  if (_state) {
    _state->increaseNesting();
  }
  return _state;
}
  
void ManagedContext::unregisterTransaction() noexcept {
  _state = nullptr; // commit is handled by transaction::Methods
}

// ============= AQLStandaloneContext =============
  
/// @brief get parent transaction (if any) increase nesting
TransactionState* AQLStandaloneContext::leaseParentTransaction() {
  if (_state) {
    _state->increaseNesting();
  }
  return _state;
}
    
/// @brief register the transaction,
void AQLStandaloneContext::registerTransaction(TransactionState* state) {
  TRI_ASSERT(_state == nullptr);
  _state = state;
  if (state) {
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->registerAQLTrx(state);
  }
}

/// @brief unregister the transaction
void AQLStandaloneContext::unregisterTransaction() noexcept {
  _state = nullptr;
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  mgr->unregisterAQLTrx(_globalId);
}

}  // namespace transaction
}  // namespace arangodb
