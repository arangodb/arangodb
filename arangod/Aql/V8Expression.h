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

#ifndef ARANGOD_AQL_V8_EXPRESSION_H
#define ARANGOD_AQL_V8_EXPRESSION_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"
#include "Aql/types.h"

#include <v8.h>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {

class AqlItemBlock;
class ExpressionContext;
class Query;
struct Variable;

struct V8Expression {

  /// @brief create the v8 expression
  V8Expression(v8::Isolate*, v8::Handle<v8::Function>, v8::Handle<v8::Object>,
               bool);

  /// @brief destroy the v8 expression
  ~V8Expression();

  /// @brief sets attribute restrictions. these prevent input variables to be
  /// fully constructed as V8 objects (which can be very expensive), but limits
  /// the objects to the actually used attributes only.
  /// For example, the expression LET x = a.value + 1 will not build the full
  /// object for "a", but only its "value" attribute
  void setAttributeRestrictions(std::unordered_map<
      Variable const*, std::unordered_set<std::string>> const&
                                    attributeRestrictions) {
    _attributeRestrictions = attributeRestrictions;
  }

  /// @brief execute the expression
  AqlValue execute(v8::Isolate* isolate, Query* query,
                    arangodb::Transaction*, ExpressionContext* context, bool& mustDestroy);

  /// @brief the isolate used when executing and destroying the expression
  v8::Isolate* isolate;
  
  /// @brief the compiled expression as a V8 function
  v8::Persistent<v8::Function> _func;
  
  /// @brief setup state
  v8::Persistent<v8::Object> _state;

  /// @brief constants
  v8::Persistent<v8::Object> _constantValues;
  
  /// @brief a Builder object, shared across calls
  std::unique_ptr<arangodb::velocypack::Builder> _builder;

  /// @brief restrictions for creating the input values
  std::unordered_map<Variable const*, std::unordered_set<std::string>>
      _attributeRestrictions;

  /// @brief whether or not the expression is simple. simple in this case means
  /// that the expression result will always contain non-cyclic data and no
  /// special JavaScript types such as Date, RegExp, Function etc.
  bool const _isSimple;
};
}
}

#endif
