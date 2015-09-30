////////////////////////////////////////////////////////////////////////////////
/// @brief AggregateNode
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_AGGREGATE_NODE_H
#define ARANGODB_AQL_AGGREGATE_NODE_H 1

#include "Basics/Common.h"
#include "Aql/AggregationOptions.h"
#include "Aql/ExecutionNode.h"
#include "Aql/types.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {
    class ExecutionBlock;
    class ExecutionPlan;
    class RedundantCalculationsReplacer;

// -----------------------------------------------------------------------------
// --SECTION--                                               class AggregateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class AggregateNode
////////////////////////////////////////////////////////////////////////////////

    class AggregateNode : public ExecutionNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class SortedAggregateBlock;
      friend class HashedAggregateBlock;
      friend class RedundantCalculationsReplacer;

      public:
       
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      public:

        AggregateNode (ExecutionPlan* plan,
                       size_t id,
                       AggregationOptions const& options,
                       std::vector<std::pair<Variable const*, Variable const*>> const& aggregateVariables,
                       Variable const* expressionVariable,
                       Variable const* outVariable,
                       std::vector<Variable const*> const& keepVariables,
                       std::unordered_map<VariableId, std::string const> const& variableMap,
                       bool count,
                       bool isDistinctCommand)
          : ExecutionNode(plan, id), 
            _options(options),
            _aggregateVariables(aggregateVariables), 
            _expressionVariable(expressionVariable),
            _outVariable(outVariable),
            _keepVariables(keepVariables),
            _variableMap(variableMap),
            _count(count), 
            _isDistinctCommand(isDistinctCommand),
            _specialized(false) {

          // outVariable can be a nullptr, but only if _count is not set
          TRI_ASSERT(! _count || _outVariable != nullptr);
        }
        
        AggregateNode (ExecutionPlan*,
                       triagens::basics::Json const& base,
                       Variable const* expressionVariable,
                       Variable const* outVariable,
                       std::vector<Variable const*> const& keepVariables,
                       std::unordered_map<VariableId, std::string const> const& variableMap,
                       std::vector<std::pair<Variable const*, Variable const*>> const& aggregateVariables,
                       bool count,
                       bool isDistinctCommand);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return AGGREGATE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node requires an additional post SORT
////////////////////////////////////////////////////////////////////////////////

        bool isDistinctCommand () const {
          return _isDistinctCommand;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node is specialized
////////////////////////////////////////////////////////////////////////////////

        bool isSpecialized () const {
          return _specialized;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the node
////////////////////////////////////////////////////////////////////////////////

        void specialized () {
          _specialized = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the aggregation method
////////////////////////////////////////////////////////////////////////////////

        AggregationOptions::AggregationMethod aggregationMethod () const {
          return _options.method;
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief set the aggregation method
////////////////////////////////////////////////////////////////////////////////

        void aggregationMethod (AggregationOptions::AggregationMethod method) {
          _options.method = method;
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief getOptions
////////////////////////////////////////////////////////////////////////////////
        
        AggregationOptions const& getOptions () const {
          return _options;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOptions
////////////////////////////////////////////////////////////////////////////////
        
        AggregationOptions& getOptions () {
          return _options;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the count flag is set
////////////////////////////////////////////////////////////////////////////////

        inline bool count () const {
          return _count;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node has an outVariable (i.e. INTO ...)
////////////////////////////////////////////////////////////////////////////////

        inline bool hasOutVariable () const {
          return _outVariable != nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* outVariable () const {
          return _outVariable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the out variable
////////////////////////////////////////////////////////////////////////////////

        void clearOutVariable () {
          TRI_ASSERT(_outVariable != nullptr);
          _outVariable = nullptr;
          _count = false;
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node has an expression variable (i.e. INTO ...
/// = expr)
////////////////////////////////////////////////////////////////////////////////

        inline bool hasExpressionVariable () const {
          return _expressionVariable != nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the expression variable
////////////////////////////////////////////////////////////////////////////////

        void setExpressionVariable (Variable const* variable) {
          TRI_ASSERT(! hasExpressionVariable());
          _expressionVariable = variable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the variable map
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<VariableId, std::string const> const& variableMap () const {
          return _variableMap;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all aggregate variables (out, in)
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::pair<Variable const*, Variable const*>> const& aggregateVariables () const {
          return _aggregateVariables;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final; 

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesSetHere () const override final {
          std::vector<Variable const*> v;
          size_t const n = _aggregateVariables.size() + (_outVariable == nullptr ? 0 : 1);
          v.reserve(n);

          for (auto const& p : _aggregateVariables) {
            v.emplace_back(p.first);
          }
          if (_outVariable != nullptr) {
            v.emplace_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief options for the aggregation
////////////////////////////////////////////////////////////////////////////////

        AggregationOptions _options;

////////////////////////////////////////////////////////////////////////////////
/// @brief input/output variables for the aggregation (out, in)
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<Variable const*, Variable const*>> _aggregateVariables;

////////////////////////////////////////////////////////////////////////////////
/// @brief input expression variable (might be null)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _expressionVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable to write to (might be null)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of variables to keep if INTO is used
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> _keepVariables;

////////////////////////////////////////////////////////////////////////////////
/// @brief map of all variable ids and names (needed to construct group data)
////////////////////////////////////////////////////////////////////////////////
                       
        std::unordered_map<VariableId, std::string const> const _variableMap;

////////////////////////////////////////////////////////////////////////////////
/// @brief COUNTing node?
////////////////////////////////////////////////////////////////////////////////

        bool _count;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node requires an additional post-SORT
////////////////////////////////////////////////////////////////////////////////

        bool const _isDistinctCommand;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node is specialized
////////////////////////////////////////////////////////////////////////////////

        bool _specialized;
    };

  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


