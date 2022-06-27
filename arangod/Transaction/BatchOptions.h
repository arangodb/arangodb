////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <memory>

namespace arangodb {

class ComputedValues;
struct ValidatorBase;

namespace aql {
class ExpressionContext;
}

namespace transaction {
class Methods;

struct BatchOptions {
  BatchOptions();
  ~BatchOptions();
  void ensureComputedValuesContext(Methods& trx);

  bool validateShardKeysOnUpdateReplace = false;
  bool validateSmartJoinAttribute = false;
  std::shared_ptr<ValidatorBase> schema;
  std::shared_ptr<ComputedValues> computedValues;
  std::unique_ptr<aql::ExpressionContext> computedValuesContext;
};

}  // namespace transaction
}  // namespace arangodb
