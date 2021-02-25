////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Context.h"

#include "Basics/StringBuffer.h"
#include "Cluster/ClusterInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {
// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler final : public VPackCustomTypeHandler {
  CustomTypeHandler(TRI_vocbase_t& vocbase, CollectionNameResolver const& resolver)
      : vocbase(vocbase), resolver(resolver) {}

  ~CustomTypeHandler() = default;

  void dump(VPackSlice const& value, VPackDumper* dumper, VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  TRI_vocbase_t& vocbase;
  CollectionNameResolver const& resolver;
};
}

/// @brief create the context
transaction::Context::Context(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase),
      _resolver(nullptr),
      _customTypeHandler(),
      _builders{_arena},
      _stringBuffer(),
      _strings{_strArena},
      _options(arangodb::velocypack::Options::Defaults),
      _transaction{TransactionId::none(), false, false},
      _ownsResolver(false) {}

/// @brief destroy the context
transaction::Context::~Context() {
  // unregister the transaction from the logfile manager
  if (_transaction.id.isSet()) {
    transaction::ManagerFeature::manager()->unregisterTransaction(_transaction.id,
                                                                  _transaction.isReadOnlyTransaction,
                                                                  _transaction.isFollowerTransaction);
  }

  // call the actual cleanup routine which frees all
  // hogged resources
  cleanup();
}
  
/// @brief destroys objects owned by the context,
/// this can be called multiple times.
/// currently called by dtor and by unit test mocks. 
/// we cannot move this into the dtor (where it was before) because
/// the mocked objects in unittests do not seem to call it and effectively leak.
void transaction::Context::cleanup() noexcept {
  // free all VPackBuilders we handed out
  for (auto& it : _builders) {
    delete it;
  }
  _builders.clear();

  // clear all strings handed out
  for (auto& it : _strings) {
    delete it;
  }
  _strings.clear();

  _stringBuffer.reset();

  if (_ownsResolver) {
    delete _resolver;
    _resolver = nullptr;
  }
}

/// @brief factory to create a custom type handler, not managed
std::unique_ptr<VPackCustomTypeHandler> transaction::Context::createCustomTypeHandler(
    TRI_vocbase_t& vocbase, CollectionNameResolver const& resolver) {
  return std::make_unique<::CustomTypeHandler>(vocbase, resolver);
}

/// @brief temporarily lease a StringBuffer object
basics::StringBuffer* transaction::Context::leaseStringBuffer(size_t initialSize) {
  if (_stringBuffer == nullptr) {
    _stringBuffer.reset(new basics::StringBuffer(initialSize, false));
  } else {
    _stringBuffer->reset();
  }

  return _stringBuffer.release();
}

/// @brief return a temporary StringBuffer object
void transaction::Context::returnStringBuffer(basics::StringBuffer* stringBuffer) noexcept {
  _stringBuffer.reset(stringBuffer);
}

/// @brief temporarily lease a std::string
std::string* transaction::Context::leaseString() {
  if (_strings.empty()) {
    // create a new string and return it
    return new std::string();
  }
  
  // re-use an existing string
  std::string* s = _strings.back();
  s->clear();
  _strings.pop_back();
  return s;
}

/// @brief return a temporary std::string object
void transaction::Context::returnString(std::string* str) noexcept {
  try {  // put string back into our vector of strings
    _strings.push_back(str);
  } catch (...) {
    // no harm done. just wipe the string
    delete str;
  }
}

/// @brief temporarily lease a Builder object
VPackBuilder* transaction::Context::leaseBuilder() {
  if (_builders.empty()) {
    // create a new builder and return it
    return new VPackBuilder();
  }

  // re-use an existing builder
  VPackBuilder* b = _builders.back();
  b->clear();
  _builders.pop_back();

  return b;
}

/// @brief return a temporary Builder object
void transaction::Context::returnBuilder(VPackBuilder* builder) noexcept {
  try {
    // put builder back into our vector of builders
    _builders.push_back(builder);
  } catch (...) {
    // no harm done. just wipe the builder
    delete builder;
  }
}

/// @brief get velocypack options with a custom type handler
VPackOptions* transaction::Context::getVPackOptions() {
  if (_customTypeHandler == nullptr) {
    // this modifies options!
    orderCustomTypeHandler();
  }

  return &_options;
}

/// @brief create a resolver
CollectionNameResolver const* transaction::Context::createResolver() {
  TRI_ASSERT(_resolver == nullptr);
  _resolver = new CollectionNameResolver(_vocbase);
  _ownsResolver = true;

  return _resolver;
}

std::shared_ptr<TransactionState> transaction::Context::createState(transaction::Options const& options) {
  // now start our own transaction
  TRI_ASSERT(vocbase().server().hasFeature<EngineSelectorFeature>());
  StorageEngine& engine = vocbase().server().getFeature<EngineSelectorFeature>().engine();
  return engine.createTransactionState(_vocbase, generateId(), options);
}

/// @brief unregister the transaction
/// this will save the transaction's id and status locally
void transaction::Context::storeTransactionResult(TransactionId id, bool wasRegistered,
                                                  bool isReadOnlyTransaction,
                                                  bool isFollowerTransaction) noexcept {
  TRI_ASSERT(_transaction.id.empty());

  if (wasRegistered) {
    _transaction.id = id;
    _transaction.isReadOnlyTransaction = isReadOnlyTransaction;
    _transaction.isFollowerTransaction = isFollowerTransaction;
  }
}

TransactionId transaction::Context::generateId() const {
  return Context::makeTransactionId();
}

std::shared_ptr<transaction::Context> transaction::Context::clone() const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "transaction::Context::clone() is not implemented");
}

/*static*/ TransactionId transaction::Context::makeTransactionId() {
  auto role = ServerState::instance()->getRole();
  if (ServerState::isCoordinator(role)) {
    return TransactionId::createCoordinator();
  } else if (ServerState::isDBServer(role)) {
    return TransactionId::createLegacy();
  }
  return TransactionId::createSingleServer();
}
