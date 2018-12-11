////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

/// @brief create the context
transaction::StandaloneContext::StandaloneContext(TRI_vocbase_t& vocbase)
  : Context(vocbase) {
}

/// @brief order a custom type handler for the collection
std::shared_ptr<arangodb::velocypack::CustomTypeHandler> transaction::StandaloneContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler.reset(
      transaction::Context::createCustomTypeHandler(_vocbase, resolver())
    );
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);

  return _customTypeHandler;
}

/// @brief return the resolver
CollectionNameResolver const& transaction::StandaloneContext::resolver() {
  if (_resolver == nullptr) {
    createResolver();
  }

  TRI_ASSERT(_resolver != nullptr);

  return *_resolver;
}

/// @brief create a context, returned in a shared ptr
/*static*/ std::shared_ptr<transaction::Context> transaction::StandaloneContext::Create(
    TRI_vocbase_t& vocbase
) {
  return std::make_shared<transaction::StandaloneContext>(vocbase);
}

} // arangodb
