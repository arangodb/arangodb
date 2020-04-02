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

#include <memory>

#include "Basics/Common.h"
#include "Containers/SmallVector.h"
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
}  // namespace velocypack

class CollectionNameResolver;
class LogicalCollection;
class TransactionState;

namespace transaction {

class Methods;

class Context {
 public:
  Context(Context const&) = delete;
  Context& operator=(Context const&) = delete;

 protected:
  /// @brief create the context
  explicit Context(TRI_vocbase_t& vocbase);

 public:
  /// @brief destroy the context
  virtual ~Context();

  /// @brief factory to create a custom type handler, not managed
  static arangodb::velocypack::CustomTypeHandler* createCustomTypeHandler(
      TRI_vocbase_t&, arangodb::CollectionNameResolver const&);

  /// @brief return the vocbase
  TRI_vocbase_t& vocbase() const { return _vocbase; }

  /// @brief temporarily lease a StringBuffer object
  basics::StringBuffer* leaseStringBuffer(size_t initialSize);

  /// @brief return a temporary StringBuffer object
  void returnStringBuffer(basics::StringBuffer* stringBuffer) noexcept;
  
  /// @brief temporarily lease a std::string
  std::string* leaseString();

  /// @brief return a temporary std::string object
  void returnString(std::string* str) noexcept;

  /// @brief temporarily lease a Builder object
  arangodb::velocypack::Builder* leaseBuilder();

  /// @brief return a temporary Builder object
  void returnBuilder(arangodb::velocypack::Builder*) noexcept;

  /// @brief get velocypack options with a custom type handler
  TEST_VIRTUAL arangodb::velocypack::Options* getVPackOptions();

  /// @brief get velocypack options for dumping
  arangodb::velocypack::Options* getVPackOptionsForDump();

  /// @brief unregister the transaction
  /// this will save the transaction's id and status locally
  void storeTransactionResult(TRI_voc_tid_t id, 
                              bool wasRegistered, bool isReadOnlyTransaction) noexcept;

 public:
  /// @brief get a custom type handler
  virtual std::shared_ptr<arangodb::velocypack::CustomTypeHandler> orderCustomTypeHandler() = 0;

  /// @brief get parent transaction (if any) increase nesting
  virtual TransactionState* getParentTransaction() const = 0;

  /// @brief whether or not the transaction is embeddable
  virtual bool isEmbeddable() const = 0;

  /// @brief register the transaction in the context
  virtual void registerTransaction(TransactionState*) = 0;

  virtual CollectionNameResolver const& resolver() = 0;

  /// @brief unregister the transaction
  virtual void unregisterTransaction() noexcept = 0;

  /// @brief generate persisted transaction ID
  virtual TRI_voc_tid_t generateId() const;
  
  /// @brief generates correct ID based on server type
  static TRI_voc_tid_t makeTransactionId();

 protected:
  /// @brief create a resolver
  CollectionNameResolver const* createResolver();

 protected:
  TRI_vocbase_t& _vocbase;
  CollectionNameResolver const* _resolver;
  std::shared_ptr<velocypack::CustomTypeHandler> _customTypeHandler;

  ::arangodb::containers::SmallVector<arangodb::velocypack::Builder*, 32>::allocator_type::arena_type _arena;
  ::arangodb::containers::SmallVector<arangodb::velocypack::Builder*, 32> _builders;

  std::unique_ptr<arangodb::basics::StringBuffer> _stringBuffer;

  ::arangodb::containers::SmallVector<std::string*, 32>::allocator_type::arena_type _strArena;
  ::arangodb::containers::SmallVector<std::string*, 32> _strings;

  arangodb::velocypack::Options _options;
  arangodb::velocypack::Options _dumpOptions;
  
 private:
  struct {
    TRI_voc_tid_t id;
    bool isReadOnlyTransaction;
  } _transaction;

  bool _ownsResolver;
};

}  // namespace transaction
}  // namespace arangodb

#endif
