////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_OPTIMIZER_RULES_FEATURE_H
#define ARANGOD_AQL_OPTIMIZER_RULES_FEATURE_H 1

#include <unordered_map>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/OptimizerRule.h"

#include <velocypack/StringRef.h>

namespace arangodb {
namespace aql {

class OptimizerRulesFeature final : public application_features::ApplicationFeature {
 public:
  explicit OptimizerRulesFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void unprepare() override final;
  
  std::vector<std::string> const& optimizerRules() const { return _optimizerRules; }

  /// @brief whether or not certain write operations can be parallelized
  bool parallelizeGatherWrites() const { return _parallelizeGatherWrites; }

  /// @brief translate a list of rule ids into rule name
  static std::vector<velocypack::StringRef> translateRules(std::vector<int> const&);

  /// @brief translate a single rule
  static velocypack::StringRef translateRule(int rule);
  
  /// @brief translate a single rule
  static int translateRule(velocypack::StringRef name);

  /// @brief return a reference to all rules
  static std::vector<OptimizerRule> const& rules() { return _rules; }

  /// @brief return a rule by its level
  static OptimizerRule& ruleByLevel(int level);
  
  /// @brief return a rule by its index
  static OptimizerRule& ruleByIndex(int index);
  
  /// @brief return the index of a rule
  static int ruleIndex(int level);
  
  /// @brief register a rule, don't call this after prepare()
  void registerRule(char const* name, RuleFunction func,
                    OptimizerRule::RuleLevel level,
                    std::underlying_type<OptimizerRule::Flags>::type flags);

 private:
  void addRules();
  void addStorageEngineRules();
  void enableOrDisableRules();
  
  std::vector<std::string> _optimizerRules;

  /// @brief if set to true, a gather node will be parallelized even for
  /// certain write operations. this is false by default, enabling it is
  /// experimental
  bool _parallelizeGatherWrites;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _fixed = false;
#endif
  
  /// @brief the rules database
  static std::vector<OptimizerRule> _rules;

  /// @brief map to look up rule id by name
  static std::unordered_map<velocypack::StringRef, int> _ruleLookup;
};

}  // namespace aql
}  // namespace arangodb

#endif
