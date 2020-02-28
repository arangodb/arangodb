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

#include "Basics/Exceptions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ticks.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {
  
SmartContext::SmartContext(TRI_vocbase_t& vocbase,
                           TRI_voc_tid_t globalId,
                           std::shared_ptr<TransactionState> state)
  : Context(vocbase), _globalId(globalId), _state(std::move(state)) {
  TRI_ASSERT(_globalId != 0);
}
  
SmartContext::~SmartContext() {
//  if (_state) {
//    if (_state->isTopLevelTransaction()) {
//      std::this_thread::sleep_for(std::chrono::seconds(60));
//      TRI_ASSERT(false); // probably should not happen
//      delete _state;
//    }
//  }
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
  return _globalId;
}
  
//  ============= ManagedContext =============
  
ManagedContext::ManagedContext(TRI_voc_tid_t globalId,
                               std::shared_ptr<TransactionState> state,
                               AccessMode::Type mode)
  : SmartContext(state->vocbase(), globalId, std::move(state)), _mode(mode) {}
  
ManagedContext::~ManagedContext() {
  if (_state != nullptr) {
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->returnManagedTrx(_globalId, _mode);
    _state = nullptr;
  }
}

/// @brief get parent transaction (if any)
std::shared_ptr<TransactionState> ManagedContext::getParentTransaction() const {
  TRI_ASSERT(_state);
  // single document transaction should never be leased out
  TRI_ASSERT(!_state->hasHint(Hints::Hint::SINGLE_OPERATION));
  return _state;
}
  
void ManagedContext::unregisterTransaction() noexcept {
  _state = nullptr; // delete is handled by transaction::Methods
}

std::shared_ptr<SmartContext> ManagedContext::clone() const {
  auto clone = std::make_shared<transaction::ManagedContext>(_globalId, _state, _mode);
  clone->_state = _state;
  return clone;
}
  
// ============= AQLStandaloneContext =============
  
/// @brief get parent transaction (if any)
std::shared_ptr<TransactionState> AQLStandaloneContext::getParentTransaction() const {
  return _state;
}
    
/// @brief register the transaction,
void AQLStandaloneContext::registerTransaction(std::shared_ptr<TransactionState> const& state) {
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
  TRI_ASSERT(_state != nullptr);
  _state = nullptr;
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  mgr->unregisterAQLTrx(_globalId);
}

std::shared_ptr<SmartContext> AQLStandaloneContext::clone() const {
  auto clone = std::make_shared<transaction::AQLStandaloneContext>(_vocbase, _globalId);
  clone->_state = _state;
  return clone;
}
  
// ============= StandaloneSmartContext =============
  
  
StandaloneSmartContext::StandaloneSmartContext(TRI_vocbase_t& vocbase)
  : SmartContext(vocbase, Context::makeTransactionId(), nullptr) {}
  
/// @brief get parent transaction (if any)
std::shared_ptr<TransactionState> StandaloneSmartContext::getParentTransaction() const {
  return _state;
}

/// @brief register the transaction,
void StandaloneSmartContext::registerTransaction(std::shared_ptr<TransactionState> const& state) {
  TRI_ASSERT(_state == nullptr);
  _state = state;
}

/// @brief unregister the transaction
void StandaloneSmartContext::unregisterTransaction() noexcept {
  TRI_ASSERT(_state != nullptr);
  _state = nullptr;
}

std::shared_ptr<SmartContext> StandaloneSmartContext::clone() const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

}  // namespace transaction
}  // namespace arangodb
