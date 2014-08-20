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
#include "Aql/Variable.h"
#include "Aql/Types.h"
#include "Aql/WalkerWorker.h"
#include "Aql/Query.h"

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
          PROJECTION              =  8, 
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
          RETURN                  = 26
        };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief factory from json.
////////////////////////////////////////////////////////////////////////////////

        static ExecutionNode* fromJsonFactory (Ast const* ast,
                                               basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor using an id
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode (size_t id) 
          : _id(id), 
            _estimatedCost(0), 
            _varUsageValid(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor using a JSON struct
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode (triagens::basics::Json const& json) 
          : ExecutionNode(triagens::basics::JsonHelper::getNumericValue<size_t>(json.json(), "id", 0)) { 
        }

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

        const std::string &getTypeString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a type of this kind; throws exception if not.
////////////////////////////////////////////////////////////////////////////////

        static void validateType (int type);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a dependency
////////////////////////////////////////////////////////////////////////////////

        void addDependency (ExecutionNode* ep) {
          _dependencies.push_back(ep);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all dependencies
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*> getDependencies () const {
          return _dependencies;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a dependency, returns true if the pointer was found and 
/// removed, please note that this does not delete ep!
////////////////////////////////////////////////////////////////////////////////

        bool removeDependency (ExecutionNode* ep) {
          auto it = _dependencies.begin(); 

          while (it != _dependencies.end()) {
            if (*it == ep) {
              _dependencies.erase(it);
              return true;
            }
            ++it;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all dependencies for the given node
////////////////////////////////////////////////////////////////////////////////

        void removeDependencies () {
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
            theClone->_dependencies.push_back((*it)->clone());
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
          if (_estimatedCost == 0){
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

        void walk (WalkerWorker<ExecutionNode>* worker);

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON, returns an AUTOFREE Json object
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

////////////////////////////////////////////////////////////////////////////////
/// @brief toJsonHelper, for a generic node
////////////////////////////////////////////////////////////////////////////////

        void fromJsonHelper (triagens::aql::Query* query,
                             basics::Json const& base);

        triagens::basics::Json toJsonHelperGeneric (
                  triagens::basics::Json& nodes,
                  TRI_memory_zone_t* zone);

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          return std::vector<Variable const*>();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
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

        std::unordered_set<Variable const*>& getVarsUsedLater () {
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

        std::unordered_set<Variable const*>& getVarsValid () {
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
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

    public:

      SingletonNode (size_t id) 
        : ExecutionNode(id) {
      }

      SingletonNode (triagens::aql::Query* query, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

      NodeType getType () const override {
        return SINGLETON;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

      virtual void toJsonHelper (triagens::basics::Json& nodes,
                                 TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        EnumerateCollectionNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

      NodeType getType () const override {
        return ENUMERATE_COLLECTION;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

      virtual void toJsonHelper (triagens::basics::Json& nodes,
                                 TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesSetHere () {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

        TRI_index_t* getIndex (std::string attr) const {
          TRI_document_collection_t* document = _collection->documentCollection();
          size_t const n = document->_allIndexes._length;
          for (size_t i = 0; i < n; ++i) {
            TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);
            for (size_t j = 0; j < idx->_fields._length; j++){
              if (idx->_fields._buffer[j] == attr) {
                //std::cout << idx->_fields._buffer[j] << "\n";
                //std::cout << "HERE\n";

                return idx;
              }
            }
          }
          return nullptr;
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
        
        EnumerateListNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return ENUMERATE_LIST;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
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

// -----------------------------------------------------------------------------
// --SECTION--                                              class IndexRangeNode
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep an upper or lower bound for the range info. Use
/// nullptr instead if you want to have no bound.
////////////////////////////////////////////////////////////////////////////////

    struct RangeInfoBound{

      RangeInfoBound(AstNode const* bound, bool include) : _include(include) {
        _bound = Json(TRI_UNKNOWN_MEM_ZONE, bound->toJson(TRI_UNKNOWN_MEM_ZONE));
      }
      
      ~RangeInfoBound(){}
      
      RangeInfoBound ( RangeInfoBound const& copy ) = delete;
      RangeInfoBound& operator= ( RangeInfoBound const& copy ) = delete;

      Json toJson () const {
        Json item(basics::Json::Array);
          item("bound", Json(TRI_UNKNOWN_MEM_ZONE, 
                TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, _bound.json())))
              ("include", Json(_include));
        return item;
      }

      Json _bound;
      bool _include;

    };
  

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep range info 
////////////////////////////////////////////////////////////////////////////////
    
    struct RangeInfo{
        
        RangeInfo ( RangeInfoBound const* low, 
                    RangeInfoBound const* high )
          : _low(low), _high(high), _valid(true) {}
        
        RangeInfo( const RangeInfo& copy ) = delete;
        RangeInfo& operator= ( RangeInfo const& copy ) = delete;

        ~RangeInfo(){
          if(_low != nullptr){
            delete _low;
          }
          if(_high != nullptr){
            delete _high;
          }
        }

        Json toJson () {
          Json item(basics::Json::Array);
          if(_low != nullptr && _high != nullptr){
            item("low", _low->toJson())
                ("high", _high->toJson())
                ("valid", Json(_valid));
          }
          else if(_low != nullptr){
            item("low", _low->toJson())
                ("valid", Json(_valid));
          }
          else if(_high != nullptr){
            item("high", _high->toJson())
                ("valid", Json(_valid));
          }
          return item;
        }
        
        std::string toString() {
          return this->toJson().toString();
        }

        RangeInfoBound const* _low;
        RangeInfoBound const* _high;
        bool _valid;

    };

// 3-way comparison: a return of -1 indicates that left is
// tighter than right, 0 that they are equal, 1 that right is tighter than
// left. For example, (x<1) is tighter than (x<=1) and (x>1) is tighter
// than (x>=1) . . .
static int CompareRangeInfoBound (RangeInfoBound const* left, RangeInfoBound const* right) {
  if (left == nullptr) {
    return (right == nullptr ? 0 : 1);
  } 
  if (right == nullptr) {
    return -1;
  }

  int cmp = TRI_CompareValuesJson(left->_bound.json(), right->_bound.json());
  if (cmp == 0 && (left->_include != right->_include)) {
    cmp = (left->_include?-1:1);
  }
  return cmp;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief class to keep a vector of RangeInfos . . .
////////////////////////////////////////////////////////////////////////////////
    
    class RangesInfo{
        
      public:
        
        RangesInfo( const RangesInfo& copy ) = delete;
        RangesInfo& operator= ( RangesInfo const& copy ) = delete;
        
        RangesInfo () : _ranges(){}

        ~RangesInfo(){}
        
        RangeInfo* find(std::string name) const {
          auto it = _ranges.find(name);
          if (it == _ranges.end()) {
            return nullptr;
          }
          return (*it).second;
        }

        /*void insert (std::string name, RangeInfo* range) {
          if( find(name) == nullptr ){
            _ranges.insert(make_pair(name, range));
          }
        }*/

        void insert (std::string name, RangeInfoBound* low, RangeInfoBound* high){ 
          auto oldRange = find(name);
          auto newRange = new RangeInfo(low, high);
          int cmp;
        
          if( oldRange == nullptr ){
            // TODO add exception . . .
            _ranges.insert(make_pair(name, newRange));
            return;
          }

          if (!oldRange->_valid) { // intersection of the empty set with any set is empty!
            return;
          }
          
          if(CompareRangeInfoBound(newRange->_low, oldRange->_high) == -1){
            oldRange->_low = newRange->_low;
          }
          
          if(CompareRangeInfoBound(newRange->_high, oldRange->_high) == -1){
            oldRange->_high = newRange->_high;
          }
          
          // check the new range bounds are valid
          if( oldRange->_low != nullptr && oldRange->_high != nullptr){
            cmp = TRI_CompareValuesJson(oldRange->_low->_bound.json(), 
                    oldRange->_high->_bound.json());
            if (cmp == 1 || (cmp == 0 && 
                  !(oldRange->_low->_include == true && oldRange->_high->_include == true ))){
              // range invalid
              oldRange->_valid = false;
            }
          }
          
        }
        
        size_t size () const {
          return _ranges.size();
        }

        Json toJson () const {
          Json list(Json::List);
          for (auto x : _ranges) {
            Json item(Json::Array);
            item("name", Json(x.first))
            ("range info", x.second->toJson());
            list(item);
          }
          return list;
        }
        
        std::string toString() const {
          return this->toJson().toString();
        }

      private: 
        std::unordered_map<std::string, RangeInfo*> _ranges; 
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

      public:

        IndexRangeNode (size_t id,
                        TRI_vocbase_t* vocbase, 
                        Collection* collection,
                        Variable const* outVariable,
                        Index* index, 
                        vector<RangeInfo*>* ranges)
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

        ~IndexRangeNode () {
          delete _ranges;
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

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        virtual ExecutionNode* clone () const {
          auto c = new IndexRangeNode(_id, _vocbase, _collection, _outVariable, _index, 
              _ranges);
          cloneDependencies(c);
          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an enumerate collection node is a multiple of the cost of
/// its unique dependency
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost () { 
          return 1;
          //FIXME improve this estimate . . .
        }
            
////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the index
////////////////////////////////////////////////////////////////////////////////

        Index* _index;

////////////////////////////////////////////////////////////////////////////////
/// @brief the range info
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<RangeInfo*>* _ranges;
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
        
        LimitNode (triagens::aql::Query* query, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return LIMIT;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        CalculationNode (triagens::aql::Query* Q, basics::Json const& base);

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

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
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

        virtual std::vector<Variable const*> getVariablesSetHere () {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
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

        SubqueryNode (Ast const* ast,
                      triagens::aql::Query* Q,
                      basics::Json const& base);

        SubqueryNode (size_t id,
                      ExecutionNode* subquery, Variable const* outVariable)
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

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere ();

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
          std::vector<Variable const*> v;
          v.push_back(_outVariable);
          return v;
        }

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
        
        FilterNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return FILTER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
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
        
        SortNode (triagens::aql::Query* query,
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

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          std::vector<Variable const*> v;
          for (auto p : _elements) {
            v.push_back(p.first);
          }
          return v;
        }

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
        
        AggregateNode (triagens::aql::Query* query,
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

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          std::vector<Variable const*> v;
          for (auto p : _aggregateVariables) {
            v.push_back(p.second);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
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

        ReturnNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return RETURN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
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

        ModificationNode (triagens::aql::Query* q,
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
        
        RemoveNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return REMOVE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
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
        
        InsertNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return INSERT;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
          std::vector<Variable const*> v;
          v.push_back(_inVariable);
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        virtual std::vector<Variable const*> getVariablesSetHere () {
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
        
        UpdateNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return UPDATE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
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

        virtual std::vector<Variable const*> getVariablesSetHere () {
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

        ReplaceNode (triagens::aql::Query* Q, basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override {
          return REPLACE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone = TRI_UNKNOWN_MEM_ZONE);

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

        virtual std::vector<Variable const*> getVariablesUsedHere () {
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

        virtual std::vector<Variable const*> getVariablesSetHere () {
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


  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


