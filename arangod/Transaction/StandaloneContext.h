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

#ifndef ARANGOD_TRANSACTION_STANDALONE_CONTEXT_H
#define ARANGOD_TRANSACTION_STANDALONE_CONTEXT_H 1

#include "SmartContext.h"

#include "Basics/Common.h"
#include "VocBase/vocbase.h"

struct TRI_vocbase_t;

namespace arangodb {
class TransactionState;

namespace transaction {

/// Can be used to reuse transaction state between multiple
/// transaction::Methods instances.
struct StandaloneContext final : public SmartContext {
  
  explicit StandaloneContext(TRI_vocbase_t& vocbase);
  
  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<TransactionState> acquireState(transaction::Options const& options,
                                                 bool& responsibleForCommit) override;
  
  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;
  
  std::shared_ptr<Context> clone() const override;
  
  /// @brief create a context, returned in a shared ptr
  static std::shared_ptr<transaction::Context> Create(TRI_vocbase_t& vocbase);
};


}  // namespace transaction
}  // namespace arangodb

#endif
