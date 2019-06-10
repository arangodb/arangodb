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

#include <map>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/OptimizerRule.h"

namespace arangodb {
namespace aql {

class OptimizerRulesFeature final : public application_features::ApplicationFeature {
  friend class Optimizer;

 public:
  explicit OptimizerRulesFeature(application_features::ApplicationServer& server);

  void prepare() override final;
  void unprepare() override final;

  /// @brief translate a list of rule ids into rule name
  static std::vector<std::string> translateRules(std::vector<int> const&);

  /// @brief translate a single rule
  static char const* translateRule(int);

  /// @brief look up the ids of all disabled rules
  static std::unordered_set<int> getDisabledRuleIds(std::vector<std::string> const&);

  /// @brief register a rule
  static void registerRule(std::string const& name, RuleFunction func,
                           OptimizerRule::RuleLevel level, bool canCreateAdditionalPlans,
                           bool canBeDisabled, bool isHidden = false);

  /// @brief register a hidden rule
  static void registerHiddenRule(std::string const& name, RuleFunction const& func,
                                 OptimizerRule::RuleLevel level,
                                 bool canCreateAdditionalPlans, bool canBeDisabled) {
    registerRule(name, func, level, canCreateAdditionalPlans, canBeDisabled, true);
  }

 private:
  void addRules();
  void addStorageEngineRules();

  static void disableRule(std::string const& name, std::unordered_set<int>& disabled);
  static void enableRule(std::string const& name, std::unordered_set<int>& disabled);

  /// @brief the rules database
  static std::map<int, OptimizerRule> _rules;

  /// @brief map to look up rule id by name
  static std::unordered_map<std::string, std::pair<int, bool>> _ruleLookup;
};

}  // namespace aql
}  // namespace arangodb

#endif
