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

namespace arangodb {

class ComputedValues;
class LogicalCollection;
struct ValidatorBase;

namespace aql {
class ExpressionContext;
}  // namespace aql

namespace transaction {
class Methods;

struct BatchOptions {
  BatchOptions();
  BatchOptions(BatchOptions const&) = delete;
  BatchOptions& operator=(BatchOptions const&) = delete;
  BatchOptions(BatchOptions&&) = default;
  BatchOptions& operator=(BatchOptions&&) = default;
  ~BatchOptions();
  void ensureComputedValuesContext(Methods& trx, LogicalCollection& collection);

  bool validateShardKeysOnUpdateReplace = false;
  bool validateSmartJoinAttribute = false;
  std::shared_ptr<ValidatorBase> schema;
  std::shared_ptr<ComputedValues> computedValues;
  std::unique_ptr<aql::ExpressionContext> computedValuesContext;
};

}  // namespace transaction
}  // namespace arangodb
