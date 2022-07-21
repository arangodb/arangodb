////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "SmartContext.h"

#include "Basics/Exceptions.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ticks.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace transaction {

SmartContext::SmartContext(TRI_vocbase_t& vocbase, TransactionId globalId,
                           std::shared_ptr<TransactionState> state)
    : Context(vocbase), _globalId(globalId), _state(std::move(state)) {
  TRI_ASSERT(_globalId.isSet());
}

SmartContext::~SmartContext() = default;

/// @brief order a custom type handler for the collection
arangodb::velocypack::CustomTypeHandler*
transaction::SmartContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler =
        transaction::Context::createCustomTypeHandler(_vocbase, resolver());
    _options.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler.get();
}

TransactionId transaction::SmartContext::generateId() const {
  return _globalId;
}

// ============= AQLStandaloneContext =============

/// @brief get transaction state, determine commit responsiblity
/*virtual*/ std::shared_ptr<TransactionState>
transaction::AQLStandaloneContext::acquireState(
    transaction::Options const& options, bool& responsibleForCommit) {
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
  auto clone =
      std::make_shared<transaction::AQLStandaloneContext>(_vocbase, _globalId);
  clone->_state = _state;
  return clone;
}

}  // namespace transaction
}  // namespace arangodb
