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

#ifndef ARANGOD_TRANSACTION_CONTEXT_H
#define ARANGOD_TRANSACTION_CONTEXT_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "VocBase/voc-types.h"

#include <velocypack/Options.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace basics {
class StringBuffer;
}

namespace velocypack {
class Builder;
struct CustomTypeHandler;
}

class CollectionNameResolver;
class LogicalCollection;
class TransactionState;

namespace transaction {
class ContextData;
class Methods;

class Context {
 public:
  Context(Context const&) = delete;
  Context& operator=(Context const&) = delete;

 protected:

  /// @brief create the context
  explicit Context(TRI_vocbase_t* vocbase);

 public:

  /// @brief destroy the context
  virtual ~Context();

  /// @brief factory to create a custom type handler, not managed
  static arangodb::velocypack::CustomTypeHandler* createCustomTypeHandler(
           TRI_vocbase_t*,
           arangodb::CollectionNameResolver const*);

  /// @brief return the vocbase
  TRI_vocbase_t* vocbase() const { return _vocbase; }
  
  /// @brief pin data for the collection
  void pinData(arangodb::LogicalCollection*);

  /// @brief whether or not the data for the collection is pinned
  bool isPinned(TRI_voc_cid_t);
  
  /// @brief temporarily lease a StringBuffer object
  basics::StringBuffer* leaseStringBuffer(size_t initialSize);

  /// @brief return a temporary StringBuffer object
  void returnStringBuffer(basics::StringBuffer* stringBuffer);
  
  /// @brief temporarily lease a Builder object
  arangodb::velocypack::Builder* leaseBuilder();
  
  /// @brief return a temporary Builder object
  void returnBuilder(arangodb::velocypack::Builder*);
  
  /// @brief get velocypack options with a custom type handler
  arangodb::velocypack::Options* getVPackOptions();
  
  /// @brief get velocypack options for dumping
  arangodb::velocypack::Options* getVPackOptionsForDump();
  
  /// @brief unregister the transaction
  /// this will save the transaction's id and status locally
  void storeTransactionResult(TRI_voc_tid_t id, bool hasFailedOperations) noexcept;
  
  /// @brief get a custom type handler
  virtual std::shared_ptr<arangodb::velocypack::CustomTypeHandler>
  orderCustomTypeHandler() = 0;

  /// @brief return the resolver
  virtual CollectionNameResolver const* getResolver() = 0;

  /// @brief get parent transaction (if any)
  virtual TransactionState* getParentTransaction() const = 0;

  /// @brief whether or not the transaction is embeddable
  virtual bool isEmbeddable() const = 0;

  /// @brief register the transaction in the context
  virtual void registerTransaction(TransactionState*) = 0;
  
  /// @brief unregister the transaction
  virtual void unregisterTransaction() noexcept = 0;

 protected:
  
  /// @brief create a resolver
  CollectionNameResolver const* createResolver();

  transaction::ContextData* contextData();
 
 protected:
  
  TRI_vocbase_t* _vocbase; 
  
  CollectionNameResolver const* _resolver;
  
  std::shared_ptr<velocypack::CustomTypeHandler> _customTypeHandler;
  
  SmallVector<arangodb::velocypack::Builder*, 32>::allocator_type::arena_type _arena;
  SmallVector<arangodb::velocypack::Builder*, 32> _builders;
  
  std::unique_ptr<arangodb::basics::StringBuffer> _stringBuffer;

  arangodb::velocypack::Options _options;
  arangodb::velocypack::Options _dumpOptions;
  
  std::unique_ptr<transaction::ContextData> _contextData;

  struct {
    TRI_voc_tid_t id; 
    bool hasFailedOperations;
  } _transaction;

  bool _ownsResolver;
};

}
}

#endif
