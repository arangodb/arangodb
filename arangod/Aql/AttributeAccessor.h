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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ATTRIBUTE_ACCESSOR_H
#define ARANGOD_AQL_ATTRIBUTE_ACCESSOR_H 1

#include "Aql/AqlValue.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AqlItemBlock;
class ExpressionContext;
struct Variable;

/// @brief AttributeAccessor
class AttributeAccessor {
 public:
  explicit AttributeAccessor(Variable const*);
  virtual ~AttributeAccessor() = default;

  /// @brief execute the accessor
  virtual AqlValue get(transaction::Methods* trx, ExpressionContext* context,
                       bool& mustDestroy) = 0;

 public:
  void replaceVariable(std::unordered_map<VariableId, Variable const*> const& replacements);

  /// @brief the attribute names vector (e.g. [ "a", "b", "c" ] for a.b.c)
  static AttributeAccessor* create(std::vector<std::string>&& path,
                                   Variable const* variable, bool dataIsFromCollection);

 protected:
  /// @brief the accessed variable
  Variable const* _variable;
};

}  // namespace aql
}  // namespace arangodb

#endif
