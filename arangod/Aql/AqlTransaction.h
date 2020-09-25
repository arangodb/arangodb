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

#ifndef ARANGOD_AQL_AQL_TRANSACTION_H
#define ARANGOD_AQL_AQL_TRANSACTION_H 1

#include "Aql/Collections.h"
#include "Transaction/Methods.h"

#include <memory>
#include <unordered_set>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace transaction {
struct Options;
}

namespace aql {
struct Collection;

class AqlTransaction : public transaction::Methods {
 public:
  /// @brief create the transaction and add all collections
  /// from the query context
  static std::unique_ptr<AqlTransaction>
    create(std::shared_ptr<transaction::Context> const& transactionContext,
           aql::Collections const& collections,
           transaction::Options const& options,
           std::unordered_set<std::string> inaccessibleCollections =
               std::unordered_set<std::string>());

  /// @brief end the transaction
  ~AqlTransaction() override = default;
  
  AqlTransaction(std::shared_ptr<transaction::Context> const& transactionContext,
                 transaction::Options const& options);

  /// protected so we can create different subclasses
  AqlTransaction(std::shared_ptr<transaction::Context> const& transactionContext,
                 aql::Collections const& collections,
                 transaction::Options const& options);

 protected:
  /// @brief add a collection to the transaction
  Result processCollection(aql::Collection&);
};
}  // namespace aql
}  // namespace arangodb

#endif
