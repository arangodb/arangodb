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

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "Context.h"

#include <memory>

struct TRI_vocbase_t;
struct TRI_v8_global_t;

namespace arangodb {

class TransactionState;

namespace transaction {

class V8Context final : public Context {
 public:
  /// @brief create the context
  V8Context(TRI_vocbase_t& vocbase, OperationOrigin operationOrigin,
            bool embeddable);

  /// @brief destroy the context
  ~V8Context() noexcept;

  /// @brief order a custom type handler
  velocypack::CustomTypeHandler* orderCustomTypeHandler() override final;

  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<TransactionState> acquireState(
      transaction::Options const& options, bool& responsibleForCommit) override;

  void enterV8Context();
  void exitV8Context() noexcept;

  /// @brief unregister the transaction from the context
  void unregisterTransaction() noexcept override;

  std::shared_ptr<Context> clone() const override;

  /// @brief whether or not the transaction is embeddable
  bool isEmbeddable() const override;

  virtual bool isV8Context() override { return true; }

  /// @brief return parent transaction state or none
  static std::shared_ptr<TransactionState> getParentState();

  /// @brief check whether the transaction is embedded
  static bool isEmbedded();

  /// @brief create a context
  static std::shared_ptr<transaction::V8Context> create(
      TRI_vocbase_t& vocbase, OperationOrigin operationOrigin, bool embeddable);

  /// @brief create a V8 transaction context if we are in a V8 isolate, and a
  /// standlone transaction context otherwise
  static std::shared_ptr<transaction::Context> createWhenRequired(
      TRI_vocbase_t& vocbase, OperationOrigin operationOrigin, bool embeddable);

 private:
  static TRI_v8_global_t* getV8State() noexcept;

  /// @brief the currently ongoing transaction
  std::shared_ptr<TransactionState> _currentTransaction;

  /// @brief whether or not further transactions can be embedded
  bool const _embeddable;
};

}  // namespace transaction
}  // namespace arangodb
