////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <memory>

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Containers/SmallVector.h"
#include "Transaction/OperationOrigin.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace velocypack {
class Builder;
struct CustomTypeHandler;
}  // namespace velocypack

class CollectionNameResolver;
class TransactionState;

namespace transaction {
class CounterGuard;
struct Options;

class Context {
 protected:
  Context(Context const&) = delete;
  Context& operator=(Context const&) = delete;

  /// @brief create the context
  explicit Context(TRI_vocbase_t& vocbase, OperationOrigin operationOrigin);

 public:
  /// @brief destroy the context
  virtual ~Context();

  /// @brief destroys objects owned by the context,
  /// this can be called multiple times.
  /// currently called by dtor and by unit test mocks.
  /// we cannot move this into the dtor (where it was before) because
  /// the mocked objects in unittests do not seem to call it and effectively
  /// leak.
  void cleanup() noexcept;

  /// @brief factory to create a custom type handler, not managed
  static std::unique_ptr<arangodb::velocypack::CustomTypeHandler>
  createCustomTypeHandler(TRI_vocbase_t&,
                          arangodb::CollectionNameResolver const&);

  /// @brief return the vocbase
  TRI_vocbase_t& vocbase() const { return _vocbase; }

  /// @brief temporarily lease a std::string
  TEST_VIRTUAL std::string* leaseString();

  /// @brief return a temporary std::string object
  TEST_VIRTUAL void returnString(std::string* str) noexcept;

  /// @brief temporarily lease a Builder object
  TEST_VIRTUAL arangodb::velocypack::Builder* leaseBuilder();

  /// @brief return a temporary Builder object
  TEST_VIRTUAL void returnBuilder(arangodb::velocypack::Builder*) noexcept;

  /// @brief get velocypack options with a custom type handler
  TEST_VIRTUAL velocypack::Options* getVPackOptions();

  /// @brief get a custom type handler
  virtual arangodb::velocypack::CustomTypeHandler* orderCustomTypeHandler() = 0;

  /// @brief get transaction state, determine commit responsiblity
  virtual std::shared_ptr<TransactionState> acquireState(
      transaction::Options const& options, bool& responsibleForCommit) = 0;

  OperationOrigin operationOrigin() const noexcept { return _operationOrigin; }

  /// @brief whether or not is from a streaming transaction (used to know
  /// whether or not can read from query cache)
  bool isStreaming() const noexcept { return _meta.isStreamingTransaction; }

  /// @brief whether or not transaction is JS (used to know
  /// whether or not can read from query cache)
  bool isTransactionJS() const noexcept { return _meta.isJStransaction; }

  bool isReadOnlyTransaction() const noexcept {
    return _meta.isReadOnlyTransaction;
  }

  void setReadOnly() noexcept { _meta.isReadOnlyTransaction = true; }

  /// @brief sets the transaction to be streaming (used to know whether or not
  /// can read from query cache)
  void setStreaming() noexcept {
    TRI_ASSERT(_meta.isJStransaction == false);
    _meta.isStreamingTransaction = true;
  }

  /// @brief sets the transaction to be JS (used to know whether or not
  /// can read from query cache)
  void setJStransaction() noexcept {
    TRI_ASSERT(_meta.isStreamingTransaction == false);
    _meta.isJStransaction = true;
  }

  /// @brief whether or not the transaction is embeddable
  virtual bool isEmbeddable() const = 0;

  CollectionNameResolver const& resolver();

  /// @brief unregister the transaction
  virtual void unregisterTransaction() noexcept = 0;

  /// @brief generate persisted transaction ID
  virtual TransactionId generateId() const;

  /// @brief only supported on some contexts
  virtual std::shared_ptr<Context> clone() const;

  virtual bool isV8Context() { return false; }

  void setCounterGuard(std::shared_ptr<CounterGuard> guard) noexcept;

  /// @brief generates correct ID based on server type
  static TransactionId makeTransactionId();

 protected:
  std::shared_ptr<TransactionState> createState(
      transaction::Options const& options);

  TRI_vocbase_t& _vocbase;
  std::unique_ptr<velocypack::CustomTypeHandler> _customTypeHandler;

  containers::SmallVector<arangodb::velocypack::Builder*, 8> _builders;
  containers::SmallVector<std::string*, 4> _strings;

  velocypack::Options _options;

  OperationOrigin _operationOrigin;

 private:
  std::unique_ptr<CollectionNameResolver> _resolver;

  std::shared_ptr<CounterGuard> _counterGuard;

  struct {
    bool isReadOnlyTransaction = false;
    bool isFollowerTransaction = false;
    bool isStreamingTransaction = false;
    bool isJStransaction = false;
  } _meta;
};

}  // namespace transaction
}  // namespace arangodb
