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

#include "Basics/Result.h"

#include <cstdint>
#include <string>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
struct AstNode;
class Expression;
class QueryContext;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

enum class RunOn : uint8_t {
  kNever = 0,
  kInsert = 1,
  kUpdate = 2,
  kReplace = 4,
};

class ComputedValues {
  class ComputedValue {
   public:
    ComputedValue(TRI_vocbase_t& vocbase, std::string_view name,
                  std::string_view expressionString, RunOn runOn,
                  bool doOverride);
    ComputedValue(ComputedValue const&) = delete;
    ComputedValue& operator=(ComputedValue const&) = delete;
    ComputedValue(ComputedValue&&) = default;
    ComputedValue& operator=(ComputedValue&&) = default;
    ~ComputedValue();

    void toVelocyPack(velocypack::Builder&) const;
    void computeAttribute(transaction::Methods& trx, velocypack::Slice input,
                          velocypack::Builder& output) const;
    bool mustRunOn(RunOn runOn) const noexcept;

   private:
    TRI_vocbase_t& _vocbase;
    std::string _name;
    std::string _expressionString;
    RunOn _runOn;
    bool _override;
    std::unique_ptr<aql::QueryContext> _queryContext;

    std::unique_ptr<aql::Expression> _expression;
    // temporary variable we will use for injecting the bind
    // parameter's (@doc) value into.
    aql::Variable const* _tempVariable;
    // root node of the expression
    aql::AstNode* _rootNode;
  };

 public:
  explicit ComputedValues(TRI_vocbase_t& vocbase,
                          std::vector<std::string> const& shardKeys,
                          velocypack::Slice params);
  ComputedValues(ComputedValues const&) = delete;
  ComputedValues& operator=(ComputedValues const&) = delete;
  ~ComputedValues();

  void toVelocyPack(velocypack::Builder&) const;

  bool mustRunOnInsert() const noexcept;
  bool mustRunOnUpdate() const noexcept;
  bool mustRunOnReplace() const noexcept;

  void computeAttributes(transaction::Methods& trx, velocypack::Slice input,
                         RunOn runOn, velocypack::Builder& output) const;

 private:
  bool mustRunOn(RunOn runOn) const noexcept;

  Result buildDefinitions(TRI_vocbase_t& vocbase,
                          std::vector<std::string> const& shardKeys,
                          velocypack::Slice params);

  // where will the computations be executed (overall)
  RunOn _runOn;

  // individual instructions for computed values
  std::vector<ComputedValue> _values;
};

}  // namespace arangodb
