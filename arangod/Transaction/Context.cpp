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

#include "Context.h"
#include "Basics/StringBuffer.h"
#include "RestServer/TransactionManagerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionManager.h"
#include "Transaction/ContextData.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler final : public VPackCustomTypeHandler {
  CustomTypeHandler(TRI_vocbase_t& vocbase, CollectionNameResolver const& resolver)
      : vocbase(vocbase), resolver(resolver) {}

  ~CustomTypeHandler() {} 

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {

    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  TRI_vocbase_t& vocbase;
  CollectionNameResolver const& resolver;
};

/// @brief create the context
transaction::Context::Context(TRI_vocbase_t& vocbase)
    : _vocbase(vocbase),
      _resolver(nullptr), 
      _customTypeHandler(),
      _builders{_arena},
      _stringBuffer(),
      _options(arangodb::velocypack::Options::Defaults),
      _dumpOptions(arangodb::velocypack::Options::Defaults),
      _transaction{ 0, false }, 
      _ownsResolver(false) {
  /// dump options contain have the escapeUnicode attribute set to true
  /// this allows dumping of string values as plain 7-bit ASCII values.
  /// for example, the string "möter" will be dumped as "m\u00F6ter".
  /// this allows faster JSON parsing in some client implementations,
  /// which speculate on ASCII strings first and only fall back to slower
  /// multibyte strings on first actual occurrence of a multibyte character.
  _dumpOptions.escapeUnicode = true;        
}

/// @brief destroy the context
transaction::Context::~Context() {
  // unregister the transaction from the logfile manager
  if (_transaction.id > 0) {
    TransactionManagerFeature::manager()->unregisterTransaction(_transaction.id, _transaction.hasFailedOperations);
  }

  // free all VPackBuilders we handed out
  for (auto& it : _builders) {
    delete it;
  }

  if (_ownsResolver) {
    delete _resolver;
  }

  _resolver = nullptr;
}

/// @brief factory to create a custom type handler, not managed
VPackCustomTypeHandler* transaction::Context::createCustomTypeHandler(TRI_vocbase_t& vocbase,
                                                                      CollectionNameResolver const& resolver) {
  return new CustomTypeHandler(vocbase, resolver);
}

/// @brief pin data for the collection
void transaction::Context::pinData(LogicalCollection* collection) {
  contextData()->pinData(collection);
}

/// @brief whether or not the data for the collection is pinned
bool transaction::Context::isPinned(TRI_voc_cid_t cid) {
  return contextData()->isPinned(cid);
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
void transaction::Context::returnStringBuffer(basics::StringBuffer* stringBuffer) {
  _stringBuffer.reset(stringBuffer);
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
void transaction::Context::returnBuilder(VPackBuilder* builder) {
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

/// @brief get velocypack options with a custom type handler for dumping
VPackOptions* transaction::Context::getVPackOptionsForDump() {
  if (_customTypeHandler == nullptr) {
    // this modifies options!
    orderCustomTypeHandler();
  }

  /// dump options have the escapeUnicode attribute set to true.
  /// this allows dumping of string values as plain 7-bit ASCII values.
  /// for example, the string "möter" will be dumped as "m\u00F6ter".
  /// this allows faster JSON parsing in some client implementations,
  /// which speculate on ASCII strings first and only fall back to slower
  /// multibyte strings on first actual occurrence of a multibyte character.
  return &_dumpOptions;
}

/// @brief create a resolver
CollectionNameResolver const* transaction::Context::createResolver() {
  TRI_ASSERT(_resolver == nullptr);
  _resolver = new CollectionNameResolver(_vocbase);
  _ownsResolver = true;

  return _resolver;
}

/// @brief unregister the transaction
/// this will save the transaction's id and status locally
void transaction::Context::storeTransactionResult(TRI_voc_tid_t id, bool hasFailedOperations) noexcept {
  TRI_ASSERT(_transaction.id == 0);

  _transaction.id = id;
  _transaction.hasFailedOperations = hasFailedOperations;
}

transaction::ContextData* transaction::Context::contextData() {
  if (_contextData == nullptr) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;

    _contextData = engine->createTransactionContextData();
  }

  return _contextData.get();
}
