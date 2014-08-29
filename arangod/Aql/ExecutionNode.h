////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionNode.h
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXECUTION_NODE_H
#define ARANGODB_AQL_EXECUTION_NODE_H 1

#include <Basics/Common.h>

#include <Basics/JsonHelper.h>
#include <VocBase/voc-types.h>
#include <VocBase/vocbase.h>

#include "Aql/Collection.h"
#include "Aql/Expression.h"
#include "Aql/Index.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Query.h"
#include "Aql/RangeInfo.h"
#include "Aql/Types.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"

#include "lib/Basics/json-utilities.h"

using Json = triagens::basics::Json;

namespace triagens {
  namespace aql {

    class ExecutionBlock;
    class ExecutionPlan;
    class Ast;

////////////////////////////////////////////////////////////////////////////////
/// @brief class ExecutionNode, abstract base class of all execution Nodes
////////////////////////////////////////////////////////////////////////////////

    class ExecutionNode {

////////////////////////////////////////////////////////////////////////////////
/// @brief node type
////////////////////////////////////////////////////////////////////////////////

      public:

        enum NodeType {
          ILLEGAL                 =  0,
          SINGLETON               =  1, 
          ENUMERATE_COLLECTION    =  2, 
          INDEX_RANGE             =  3,
          ENUMERATE_LIST          =  4, 
          FILTER                  =  5, 
          LIMIT                   =  6, 
          INTERSECTION            =  7,
          CALCULATION             =  9, 
          SUBQUERY                = 10, 
          SORT                    = 11, 
          AGGREGATE               = 12, 
          LOOKUP_JOIN             = 13,
          MERGE_JOIN              = 14,
          LOOKUP_INDEX_UNIQUE     = 15,
          LOOKUP_INDEX_RANGE      = 17,
          LOOKUP_FULL_COLLECTION  = 18,
          CONCATENATION           = 19,
          MERGE                   = 20,
          REMOTE                  = 21,
          INSERT                  = 22,
          REMOVE                  = 23,
          REPLACE                 = 24,
          UPDATE                  = 25,
          RETURN                  = 26,
          NORESULTS               = 27
        };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor using an id
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode (size_t id)
          : _id(id), 
            _estimatedCost(0.0), 
            _varUsageValid(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor using a JSON struct
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode (triagens::basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor, free dependencies;
////////////////////////////////////////////////////////////////////////////////

        virtual ~ExecutionNode () { 
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief factory from json.
////////////////////////////////////////////////////////////////////////////////

        static ExecutionNode* fromJsonFactory (Ast* ast,
                                               basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the node's id
////////////////////////////////////////////////////////////////////////////////

        inline size_t id () const {
          return _id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        virtual NodeType getType () const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name of the node
////////////////////////////////////////////////////////////////////////////////

        std::string const& getTypeString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a type of this kind; throws exception if not.
////////////////////////////////////////////////////////////////////////////////

        static void validateType (int type);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a dependency
////////////////////////////////////////////////////////////////////////////////

        void addDependency (ExecutionNode* ep) {
          _dependencies.push_back(ep);
          ep->_parents.push_back(this);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all dependencies
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*> getDependencies () const {
          return _dependencies;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all parents
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*> getParents () const {
          return _parents;
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief inspect one index; only skiplist indices which match attrs in sequence.
/// @returns a a qualification how good they match;
///      match->index==nullptr means no match at all.
////////////////////////////////////////////////////////////////////////////////
        enum MatchType {
          FULL_MATCH,
          REVERSE_MATCH,
          NOT_COVERED_IDX,
          NOT_COVERED_ATTR,
          NO_MATCH
        };

        struct IndexMatch{
          TRI_index_t* index;     // The index concerned; if null, this is a nonmatch.
          vector<MatchType> Match;// qualification of the attrs match quality
          bool fullmatch;         // do all critereons match
        };

        typedef std::vector<std::pair<std::string, bool>> IndexMatchVec;

        static IndexMatch CompareIndex (TRI_index_t* idx,
                                        IndexMatchVec &attrs);


////////////////////////////////////////////////////////////////////////////////
/// @brief replace a dependency, returns true if the pointer was found and 
/// replaced, please note that this does not delete oldNode!
////////////////////////////////////////////////////////////////////////////////

        bool replaceDependency (ExecutionNode* oldNode, ExecutionNode* newNode) {
          auto it = _dependencies.begin(); 

          while (it != _dependencies.end()) {
            if (*it == oldNode) {
              *it = newNode;
              try {
                newNode->_parents.push_back(this);
              }
              catch (...) {
                *it = oldNode;  // roll back
                return false;
              }
              try {
                for (auto it2 = oldNode->_parents.begin();
                     it2 != oldNode->_parents.end();
                     ++it2) {
                  if (*it2 == this) {
                    oldNode->_parents.erase(it2);
                    break;
                  }
                }
              }
              catch (...) {
                // If this happens, we ignore that the _parents of oldNode
                // are not set correctly
              }
              return true;
            }
            ++it;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a dependency, returns true if the pointer was found and 
/// removed, please note that this does not delete ep!
////////////////////////////////////////////////////////////////////////////////

        bool removeDependency (ExecutionNode* ep) {
          bool ok = false;
          for (auto it = _dependencies.begin();
               it != _dependencies.end();
               ++it) {
            if (*it == ep) {
              try {
                _dependencies.erase(it);
              }
              catch (...) {
                return false;
              }
              ok = true;
              break;
            }
          }
          if (! ok) {
            return false;
          }

          // Now remove us as a parent of the old dependency as well:
          for (auto it = ep->_parents.begin(); 
               it != ep->_parents.end(); 
               ++it) {
            if (*it == this) {
              try {
                ep->_parents.erase(it);
              }
              catch (...) {
              }
              return true;
            }
          }

          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all dependencies for the given node
////////////////////////////////////////////////////////////////////////////////

        void removeDependencies () {
          for (auto x : _dependencies) {
            for (auto it = x->_parents.begin();
                 it != x->_parents.end();
                 ++it) {
              if (*it == this) {
                try {
                  x->_parents.erase(it);
                }
                catch (...) {
                }
                break;
              }
            }
          }
          _dependencies.clear();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone execution Node recursively, this makes the class abstract
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const = 0;   // make class abstract

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for cloning, use virtual clone methods for dependencies
////////////////////////////////////////////////////////////////////////////////

        void cloneDependencies (ExecutionNode* theClone) const {
          auto it = _dependencies.begin();
          while (it != _dependencies.end()) {
            auto c = (*it)->clone();
            try {
              c->_parents.push_back(theClone);
              theClone->_dependencies.push_back(c);
            }
            catch (...) {
              delete c;
              throw;
            }
            ++it;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to a string, basically for debugging purposes
////////////////////////////////////////////////////////////////////////////////

        virtual void appendAsString (std::string& st, int indent = 0);
       
////////////////////////////////////////////////////////////////////////////////
/// @brief estimate the cost of the node . . .
////////////////////////////////////////////////////////////////////////////////
        
        double getCost () {
          if (_estimatedCost == 0.0) {
            _estimatedCost = estimateCost();
          }
          return _estimatedCost;
        };

        virtual double estimateCost () = 0;
        
        //TODO nodes should try harder to estimate their own cost, i.e. the cost
        //of performing the operation of the node . . .

////////////////////////////////////////////////////////////////////////////////
/// @brief walk a complete execution plan recursively
////////////////////////////////////////////////////////////////////////////////

        bool walk (WalkerWorker<ExecutionNode>* worker);

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON, returns an AUTOFREE Json object
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t*,
                                       bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          return std::vector<Variable const*>();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          return std::vector<Variable const*>();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setVarsUsedLater
////////////////////////////////////////////////////////////////////////////////

        void setVarsUsedLater (std::unordered_set<Variable const*>& v) {
          _varsUsedLater = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVarsUsedLater
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<Variable const*> const& getVarsUsedLater () const {
          TRI_ASSERT(_varUsageValid);
          return _varsUsedLater;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setVarsValid
////////////////////////////////////////////////////////////////////////////////

        void setVarsValid (std::unordered_set<Variable const*>& v) {
          _varsValid = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVarsValid
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<Variable const*> const& getVarsValid () const {
          TRI_ASSERT(_varUsageValid);
          return _varsValid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setVarUsageValid
////////////////////////////////////////////////////////////////////////////////

        void setVarUsageValid () {
          _varUsageValid = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidateVarUsage
////////////////////////////////////////////////////////////////////////////////

        void invalidateVarUsage () {
          _varsUsedLater.clear();
          _varsValid.clear();
          _varUsageValid = false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief can the node throw?
////////////////////////////////////////////////////////////////////////////////

        virtual bool canThrow () {
          return false;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------
      
      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief factory for (optional) variables from json.
////////////////////////////////////////////////////////////////////////////////

        static Variable* varFromJson (Ast*,
                                      triagens::basics::Json const& base,
                                      const char *variableName,
                                      bool optional = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief toJsonHelper, for a generic node
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJsonHelperGeneric (triagens::basics::Json&,
                                                    TRI_memory_zone_t*,
                                                    bool) const;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------
      
      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief node id
////////////////////////////////////////////////////////////////////////////////

        size_t const _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief our dependent nodes
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*> _dependencies;

////////////////////////////////////////////////////////////////////////////////
/// @brief our parent nodes
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*> _parents;

////////////////////////////////////////////////////////////////////////////////
/// @brief NodeType to string mapping
////////////////////////////////////////////////////////////////////////////////
        
        static std::unordered_map<int, std::string const> const TypeNames;

////////////////////////////////////////////////////////////////////////////////
/// @brief _estimatedCost = 0 if uninitialised and otherwise stores the result
/// of estimateCost()
////////////////////////////////////////////////////////////////////////////////

        double _estimatedCost;

////////////////////////////////////////////////////////////////////////////////
/// @brief _varsUsedLater and _varsValid, the former contains those
/// variables that are still needed further down in the chain. The
/// latter contains the variables that are set from the dependent nodes
/// when an item comes into the current node. Both are only valid if
/// _varUsageValid is true. Use ExecutionPlan::findVarUsage to set
/// this.
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<Variable const*> _varsUsedLater;

        std::unordered_set<Variable const*> _varsValid;

        bool _varUsageValid;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                               class SingletonNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class SingletonNode
////////////////////////////////////////////////////////////////////////////////

    class SingletonNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class SingletonBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
 
        SingletonNode (size_t id) 
          : ExecutionNode(id) {
        }

        SingletonNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return SINGLETON;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new SingletonNode(_id);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a singleton is 1
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1;
        }

    };

// -----------------------------------------------------------------------------
// --SECTION--                                     class EnumerateCollectionNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class EnumerateCollectionNode
////////////////////////////////////////////////////////////////////////////////

    class EnumerateCollectionNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class EnumerateCollectionBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        EnumerateCollectionNode (size_t id,
                                 TRI_vocbase_t* vocbase, 
                                 Collection* collection,
                                 Variable const* outVariable)
          : ExecutionNode(id), 
            _vocbase(vocbase), 
            _collection(collection),
            _outVariable(outVariable){
          TRI_ASSERT(_vocbase != nullptr);
          TRI_ASSERT(_collection != nullptr);
          TRI_ASSERT(_outVariable != nullptr);
        }

        EnumerateCollectionNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return ENUMERATE_COLLECTION;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new EnumerateCollectionNode(_id, _vocbase, _collection, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an enumerate collection node is a multiple of the cost of
/// its unique dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () { 
          return static_cast<double>(_collection->count()) * _dependencies.at(0)->getCost(); 
          //FIXME improve this estimate . . .
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get vector of indices that has any match in its fields with <attrs> 
////////////////////////////////////////////////////////////////////////////////

        void getIndexesForIndexRangeNode (std::unordered_set<std::string> attrs, 
           std::vector<TRI_index_t*>& idxs, std::vector<size_t>& prefixes) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get vector of skiplist indices which match attrs in sequence.
/// @returns a list of indexes with a qualification how good they match 
///    the specified indexes.
////////////////////////////////////////////////////////////////////////////////

        std::vector<IndexMatch> getIndicesOrdered (IndexMatchVec &attrs) const;

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

        Collection* collection () const {
          return _collection;
        }

        Variable const* outVariable () const {
          return _outVariable;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                           class EnumerateListNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

    class EnumerateListNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class EnumerateListBlock;
      friend struct VarUsageFinder;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      public:

        EnumerateListNode (size_t id,
                           Variable const* inVariable,
                           Variable const* outVariable) 
          : ExecutionNode(id), 
            _inVariable(inVariable), 
            _outVariable(outVariable) {

          TRI_ASSERT(_inVariable != nullptr);
          TRI_ASSERT(_outVariable != nullptr);
        }
        
        EnumerateListNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return ENUMERATE_LIST;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new EnumerateListNode(_id, _inVariable, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an enumerate list node is . . . FIXME
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1000 * _dependencies.at(0)->getCost(); 
          //FIXME improve this estimate . . .
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable to read from
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable to write to
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

////////////////////////////////////////////////////////////////////////////////
/// @brief class IndexRangeNode
////////////////////////////////////////////////////////////////////////////////

    class IndexRangeNode: public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class IndexRangeBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

// _ranges must correspond to a prefix of the fields of the index <index>, i.e.
// _ranges.at(i) is a range of values for idx->_fields._buffer[i]. 

      public:

        IndexRangeNode (size_t id,
                        TRI_vocbase_t* vocbase, 
                        Collection* collection,
                        Variable const* outVariable,
                        TRI_index_t* index, 
                        std::vector<std::vector<RangeInfo*>> const& ranges)
          : ExecutionNode(id), 
            _vocbase(vocbase), 
            _collection(collection),
            _outVariable(outVariable),
            _index(index),
            _ranges(ranges) {
          TRI_ASSERT(_vocbase != nullptr);
          TRI_ASSERT(_collection != nullptr);
          TRI_ASSERT(_outVariable != nullptr);
          TRI_ASSERT(_index != nullptr);
        }

        IndexRangeNode (Ast*, basics::Json const& base);

        ~IndexRangeNode () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return INDEX_RANGE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          std::vector<std::vector<RangeInfo*>> ranges;
          for (size_t i = 0; i < _ranges.size(); i++){
            for (auto x: _ranges.at(i)){
              ranges.at(i).push_back(x);
            }
          }
          auto c = new IndexRangeNode(_id, _vocbase, _collection, _outVariable,
              _index, ranges);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an index range node is a multiple of the cost of
/// its unique dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () { 
          // the cost of the enumerate collection node we are replacing . . .
          /* cost = static_cast<double>(_collection->count()) * 
            _dependencies.at(0)->getCost();

          if (_index->_type == TRI_IDX_TYPE_HASH_INDEX) {


          }*/
          
          // TODO: take into accout that we might have the range -inf ... +inf
          // if we use the index only for sorting 
          return 1;
          //FIXME improve this estimate . . .
        }
            
////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the pattern matches this nodes index
////////////////////////////////////////////////////////////////////////////////

        bool MatchesIndex (IndexMatchVec pattern) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief the index
////////////////////////////////////////////////////////////////////////////////

        TRI_index_t* _index;

////////////////////////////////////////////////////////////////////////////////
/// @brief the range info
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<std::vector<RangeInfo*>> _ranges;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                   class LimitNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class LimitNode
////////////////////////////////////////////////////////////////////////////////

    class LimitNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class LimitBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors for various arguments, always with offset and limit
////////////////////////////////////////////////////////////////////////////////

      public:

        LimitNode (size_t id,
                   size_t offset, 
                   size_t limit) 
          : ExecutionNode(id), 
            _offset(offset), 
            _limit(limit) {
        }

        LimitNode (size_t id,
                   size_t limit) 
          : ExecutionNode(id), 
            _offset(0), 
            _limit(limit) {
        }
        
        LimitNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return LIMIT;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new LimitNode(_id, _offset, _limit);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a limit node is the minimum of the _limit, and the cost
/// the dependency . . .
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1.005 * std::min(static_cast<double>(_limit), 
            _dependencies.at(0)->getCost());
          //FIXME improve this estimate . . .
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to know the offset and limit
////////////////////////////////////////////////////////////////////////////////

      private:

        size_t _offset;
        size_t _limit;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                             class CalculationNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class CalculationNode
////////////////////////////////////////////////////////////////////////////////

    class CalculationNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class CalculationBlock;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        CalculationNode (size_t id,
                         Expression* expr, 
                         Variable const* outVariable)
          : ExecutionNode(id), 
            _expression(expr), 
            _outVariable(outVariable) {

          TRI_ASSERT(_expression != nullptr);
          TRI_ASSERT(_outVariable != nullptr);
        }

        CalculationNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~CalculationNode () {
          if (_expression != nullptr) {
            delete _expression;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return CALCULATION;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new CalculationNode(_id, _expression->clone(), _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* outVariable () const {
          return _outVariable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the expression
////////////////////////////////////////////////////////////////////////////////

        Expression* expression () const {
          return _expression;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a calculation node is the cost of the unique dependency
//  times a constant
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 2 * _dependencies.at(0)->getCost(); 
          //FIXME improve this estimate . . . 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::unordered_set<Variable*> vars = _expression->variables();
          std::vector<Variable const*> v;
          for (auto vv : vars) {
            v.push_back(vv);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief can the node throw?
////////////////////////////////////////////////////////////////////////////////

        bool canThrow () {
          return _expression->canThrow();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to have an expression and where to write the result
////////////////////////////////////////////////////////////////////////////////

        Expression* _expression;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable to write to
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                class SubqueryNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class SubqueryNode
////////////////////////////////////////////////////////////////////////////////

    class SubqueryNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class SubqueryBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      public:

        SubqueryNode (Ast*,
                      basics::Json const& base);

        SubqueryNode (size_t id,
                      ExecutionNode* subquery, 
                      Variable const* outVariable)
          : ExecutionNode(id), 
            _subquery(subquery), 
            _outVariable(outVariable) {

          TRI_ASSERT(_subquery != nullptr);
          TRI_ASSERT(_outVariable != nullptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return SUBQUERY;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new SubqueryNode(_id, _subquery->clone(), _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for subquery
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* getSubquery () {
          return _subquery;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setter for subquery
////////////////////////////////////////////////////////////////////////////////

        void setSubquery (ExecutionNode* subquery) {
          TRI_ASSERT(subquery != nullptr);
          TRI_ASSERT(_subquery == nullptr); // do not allow overwriting an existing subquery
          _subquery = subquery;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a subquery node is the cost of its unique dependency
/// times a small constant
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1.005 * _dependencies.at(0)->getCost();
          //FIXME improve this estimate . . .
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief can the node throw? Note that this means that an exception can
/// *originate* from this node. That is, this method does not need to
/// return true just because a dependent node can throw an exception.
////////////////////////////////////////////////////////////////////////////////

        bool canThrow ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to have an expression and where to write the result
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* _subquery;

////////////////////////////////////////////////////////////////////////////////
/// @brief variable to write to
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class FilterNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class FilterNode
////////////////////////////////////////////////////////////////////////////////

    class FilterNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class FilterBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors for various arguments, always with offset and limit
////////////////////////////////////////////////////////////////////////////////

      public:

        FilterNode (size_t id,
                    Variable const* inVariable)
          : ExecutionNode(id), 
            _inVariable(inVariable) {

          TRI_ASSERT(_inVariable != nullptr);
        }
        
        FilterNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return FILTER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new FilterNode(_id, _inVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a filter node is . . . FIXME
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return _dependencies.at(0)->getCost() * 0.105;
          //FIXME! 0.005 is the cost of doing the filter node under the
          //assumption that it returns 10% of the results of its dependency
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable to read from
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                            struct SortInformation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief this is an auxilliary struct for processed sort criteria information
////////////////////////////////////////////////////////////////////////////////
    
    struct SortInformation {
      std::vector<std::tuple<ExecutionNode const*, std::string, bool>> criteria;
      bool isValid   = true;
      bool isComplex = false;
          
      bool isCoveredBy (SortInformation const& other) {
        if (! isValid || ! other.isValid) {
          return false;
        }

        if (isComplex) {
          return false;
        }

        size_t const n = criteria.size();
        for (size_t i = 0; i < n; ++i) {
          if (other.criteria.size() < i) {
            return false;
          }

          auto ours = criteria[i];
          auto theirs = other.criteria[i];

          if (std::get<2>(ours) != std::get<2>(theirs)) {
            // sort order is different
            return false;
          }

          if (std::get<1>(ours) != std::get<1>(theirs)) {
            // sort criterion is different
            return false;
          }
        }

        return true;
      }
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    class SortNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class SortNode
////////////////////////////////////////////////////////////////////////////////

    class SortNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class SortBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      public:

        SortNode (size_t id,
                  std::vector<std::pair<Variable const*, bool>> elements)
          : ExecutionNode(id), 
            _elements(elements) {
        }
        
        SortNode (Ast*,
                  basics::Json const& base,
                  std::vector<std::pair<Variable const*, bool>> elements);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return SORT;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new SortNode(_id, _elements);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a sort node is . . . FIXME
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          double depCost = _dependencies.at(0)->getCost();
          return log(depCost) * depCost;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          for (auto p : _elements) {
            v.push_back(p.first);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get Variables Used Here including ASC/DESC
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<Variable const*, bool>> getElements () const {
          return _elements;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all sort information 
////////////////////////////////////////////////////////////////////////////////

        SortInformation getSortInformation (ExecutionPlan*) const;

        std::vector<std::pair<ExecutionNode*, bool>> getCalcNodePairs ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of variable and sort direction
/// (true = ascending | false = descending)
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<Variable const*, bool>> _elements;

    };


// -----------------------------------------------------------------------------
// --SECTION--                                               class AggregateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class AggregateNode
////////////////////////////////////////////////////////////////////////////////

    class AggregateNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class AggregateBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      public:

        AggregateNode (size_t id,
                       std::vector<std::pair<Variable const*, Variable const*>> aggregateVariables,
                       Variable const* outVariable,
                       std::unordered_map<VariableId, std::string const> const& variableMap)
          : ExecutionNode(id), 
            _aggregateVariables(aggregateVariables), 
            _outVariable(outVariable),
            _variableMap(variableMap) {
          // outVariable can be a nullptr
        }
        
        AggregateNode (Ast*,
                       basics::Json const& base,
                       Variable const* outVariable,
                       std::unordered_map<VariableId, std::string const> const& variableMap,
                       std::vector<std::pair<Variable const*, Variable const*>> aggregateVariables);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return AGGREGATE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new AggregateNode(_id, _aggregateVariables, _outVariable, _variableMap);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an aggregate node is . . . FIXME
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 2 * _dependencies.at(0)->getCost();
          //FIXME improve this estimate . . .
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          for (auto p : _aggregateVariables) {
            v.push_back(p.first);
          }
          if (_outVariable != nullptr) {
            v.push_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input/output variables for the aggregation (out, in)
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<Variable const*, Variable const*>> _aggregateVariables;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable to write to (might be null)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief map of all variable ids and names (needed to construct group data)
////////////////////////////////////////////////////////////////////////////////
                       
        std::unordered_map<VariableId, std::string const> const _variableMap;

    };


// -----------------------------------------------------------------------------
// --SECTION--                                                  class ReturnNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class ReturnNode
////////////////////////////////////////////////////////////////////////////////

    class ReturnNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class ReturnBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructors for various arguments, always with offset and limit
////////////////////////////////////////////////////////////////////////////////

      public:

        ReturnNode (size_t id,
                    Variable const* inVariable)
          : ExecutionNode(id), 
            _inVariable(inVariable) {

          TRI_ASSERT(_inVariable != nullptr);
        }

        ReturnNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return RETURN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new ReturnNode(_id, _inVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a return node is the cost of its only dependency . . .
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return _dependencies.at(0)->getCost();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to know the offset and limit
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                            class ModificationNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for modification operations
////////////////////////////////////////////////////////////////////////////////

    class ModificationNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class ModificationBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection and options
////////////////////////////////////////////////////////////////////////////////

      protected:

        ModificationNode (size_t id,
                          TRI_vocbase_t* vocbase, 
                          Collection* collection,
                          ModificationOptions const& options)
          : ExecutionNode(id), 
            _vocbase(vocbase), 
            _collection(collection),
            _options(options) {

          TRI_ASSERT(_vocbase != nullptr);
          TRI_ASSERT(_collection != nullptr);
        }

        ModificationNode (Ast*,
                          basics::Json const& json);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief _vocbase, the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief modification operation options
////////////////////////////////////////////////////////////////////////////////

        ModificationOptions _options;

    };


// -----------------------------------------------------------------------------
// --SECTION--                                                  class RemoveNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class RemoveNode
////////////////////////////////////////////////////////////////////////////////

    class RemoveNode : public ModificationNode {
      
      friend class ExecutionBlock;
      friend class RemoveBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor 
////////////////////////////////////////////////////////////////////////////////

      public:

        RemoveNode (size_t id,
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inVariable,
                    Variable const* outVariable)
          : ModificationNode(id, vocbase, collection, options),
            _inVariable(inVariable),
            _outVariable(outVariable) {

          TRI_ASSERT(_inVariable != nullptr);
          // _outVariable might be a nullptr
        }
        
        RemoveNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return REMOVE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new RemoveNode(_id, _vocbase, _collection, _options, _inVariable, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a remove node is a multiple of the cost of its unique 
/// dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return _dependencies.at(0)->getCost();
          // TODO: improve this estimate!
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          if (_outVariable != nullptr) {
            v.push_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable (might be a nullptr)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class InsertNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class InsertNode
////////////////////////////////////////////////////////////////////////////////

    class InsertNode : public ModificationNode {
      
      friend class ExecutionBlock;
      friend class InsertBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor 
////////////////////////////////////////////////////////////////////////////////

      public:

        InsertNode (size_t id,
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inVariable,
                    Variable const* outVariable)
          : ModificationNode(id, vocbase, collection, options),
            _inVariable(inVariable),
            _outVariable(outVariable) {

          TRI_ASSERT(_inVariable != nullptr);
          // _outVariable might be a nullptr
        }
        
        InsertNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return INSERT;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new InsertNode(_id, _vocbase, _collection, _options, _inVariable, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a remove node is a multiple of the cost of its unique 
/// dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1000 * _dependencies.at(0)->getCost(); //FIXME change this!
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          if (_outVariable != nullptr) {
            v.push_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class UpdateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class UpdateNode
////////////////////////////////////////////////////////////////////////////////

    class UpdateNode : public ModificationNode {
      
      friend class ExecutionBlock;
      friend class UpdateBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        UpdateNode (size_t id, 
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inDocVariable,
                    Variable const* inKeyVariable,
                    Variable const* outVariable)
          : ModificationNode(id, vocbase, collection, options),
            _inDocVariable(inDocVariable),
            _inKeyVariable(inKeyVariable),
            _outVariable(outVariable) {

          TRI_ASSERT(_inDocVariable != nullptr);
          // _inKeyVariable might be a nullptr
          // _outVariable might be a nullptr
        }
        
        UpdateNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return UPDATE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new UpdateNode(_id, _vocbase, _collection, _options, _inDocVariable, _inKeyVariable, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a remove node is a multiple of the cost of its unique 
/// dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1000 * _dependencies.at(0)->getCost(); //FIXME change this!
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inDocVariable);

          if (_inKeyVariable != nullptr) {
            v.push_back(_inKeyVariable);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          if (_outVariable != nullptr) {
            v.push_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for documents
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inDocVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for keys
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inKeyVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ReplaceNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class ReplaceNode
////////////////////////////////////////////////////////////////////////////////

    class ReplaceNode : public ModificationNode {
      
      friend class ExecutionBlock;
      friend class ReplaceBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        ReplaceNode (size_t id,
                     TRI_vocbase_t* vocbase, 
                     Collection* collection,
                     ModificationOptions const& options,
                     Variable const* inDocVariable,
                     Variable const* inKeyVariable,
                     Variable const* outVariable)
          : ModificationNode(id, vocbase, collection, options),
            _inDocVariable(inDocVariable),
            _inKeyVariable(inKeyVariable),
            _outVariable(outVariable) {

          TRI_ASSERT(_inDocVariable != nullptr);
          // _inKeyVariable might be a nullptr
          // _outVariable might be a nullptr
        }

        ReplaceNode (Ast*, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return REPLACE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new ReplaceNode(_id, _vocbase, _collection, _options, _inDocVariable, _inKeyVariable, _outVariable);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a remove node is a multiple of the cost of its unique 
/// dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 1000 * _dependencies.at(0)->getCost(); //FIXME change this!
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () const {
          std::vector<Variable const*> v;
          v.push_back(_inDocVariable);

          if (_inKeyVariable != nullptr) {
            v.push_back(_inKeyVariable);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () const {
          std::vector<Variable const*> v;
          if (_outVariable != nullptr) {
            v.push_back(_outVariable);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for documents
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inDocVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for keys
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inKeyVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                               class NoResultsNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class NoResultsNode
////////////////////////////////////////////////////////////////////////////////

    class NoResultsNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class NoResultsBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
  
        NoResultsNode (size_t id) 
          : ExecutionNode(id) {
        }

        NoResultsNode (Ast*, basics::Json const& base)
          : ExecutionNode(base) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return NORESULTS;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json&,
                                   TRI_memory_zone_t*,
                                   bool) const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new NoResultsNode(_id);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a NoResults is 0
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () {
          return 0;
        }

    };

  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


