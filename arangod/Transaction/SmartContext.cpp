////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2020 ArangoDB GmbH, Cologne, Germany
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
arangodb::velocypack::CustomTypeHandler* transaction::SmartContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler =
        transaction::Context::createCustomTypeHandler(_vocbase, resolver());
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler.get();
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
                               bool responsibleForCommit, bool cloned)
  : SmartContext(state->vocbase(), globalId, state),
    _responsibleForCommit(responsibleForCommit), _cloned(cloned) {}
  
ManagedContext::~ManagedContext() {
  if (_state != nullptr && !_cloned) {
    TRI_ASSERT(!_responsibleForCommit);
    
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->returnManagedTrx(_globalId);
    _state = nullptr;
  }
}

/// @brief get transaction state, determine commit responsiblity
/*virtual*/ std::shared_ptr<TransactionState> transaction::ManagedContext::acquireState(transaction::Options const& options,
                                                                                        bool& responsibleForCommit) {
  TRI_ASSERT(_state);
  // single document transaction should never be leased out
  TRI_ASSERT(!_state->hasHint(Hints::Hint::SINGLE_OPERATION));
  responsibleForCommit = _responsibleForCommit;
  return _state;
}
  
void ManagedContext::unregisterTransaction() noexcept {
  TRI_ASSERT(_responsibleForCommit);
  _state = nullptr;
}

std::shared_ptr<transaction::Context> ManagedContext::clone() const {
  // cloned transactions may never be responsible for commits
  auto clone = std::make_shared<transaction::ManagedContext>(_globalId, _state,
                                                             /*responsibleForCommit*/false, /*cloned*/true);
  clone->_state = _state;
  return clone;
}
  
// ============= AQLStandaloneContext =============

/// @brief get transaction state, determine commit responsiblity
/*virtual*/ std::shared_ptr<TransactionState> transaction::AQLStandaloneContext::acquireState(transaction::Options const& options,
                                                                                              bool& responsibleForCommit) {
  if (!_state) {
    responsibleForCommit = true;
    _state = transaction::Context::createState(options);
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->registerAQLTrx(_state);
  } else {
    responsibleForCommit = false;
  }

  return _state;
}

/// @brief unregister the transaction
void AQLStandaloneContext::unregisterTransaction() noexcept {
  TRI_ASSERT(_state != nullptr);
  _state = nullptr;
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  mgr->unregisterAQLTrx(_globalId);
}

std::shared_ptr<transaction::Context> AQLStandaloneContext::clone() const {
  auto clone = std::make_shared<transaction::AQLStandaloneContext>(_vocbase, _globalId);
  clone->_state = _state;
  return clone;
}

}  // namespace transaction
}  // namespace arangodb
