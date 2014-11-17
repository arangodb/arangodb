////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for RangeInfo
///
/// @file arangod/Aql/RangeInfo.h
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
/// @author not James
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_RANGE_INFO_H
#define ARANGODB_AQL_RANGE_INFO_H 1

#include <Basics/Common.h>
#include <Basics/JsonHelper.h>

#include "Aql/AstNode.h"

#include "Basics/json-utilities.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                              class RangeInfoBound
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep an upper or lower bound named _bound and a bool
/// _include which indicates if the _bound is included or not. This can
/// hold a constant value (NODE_TYPE_VALUE) or a subexpression, which is
/// indicated by the boolean flag _isConstant. The whole thing can be
/// not _defined, which counts as _isConstant.
/// We have the following invariants:
///  (! _defined) ==> _isConstant
////////////////////////////////////////////////////////////////////////////////

    struct RangeInfoBound {

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        RangeInfoBound (AstNode const* bound, bool include) 
          : _bound(), 
            _include(include), 
            _defined(false),
            _expressionAst(nullptr) {

          if (bound->type == NODE_TYPE_VALUE) {
            _bound = triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                            bound->toJsonValue(TRI_UNKNOWN_MEM_ZONE));
            _isConstant = true;
          }
          else {
            _bound = triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                            bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
            _isConstant = false;
            _expressionAst = bound;
          }
          _defined = true;
        }
        
        RangeInfoBound (triagens::basics::Json const& json) 
          : _bound(),
            _include(basics::JsonHelper::checkAndGetBooleanValue(json.json(),
                                                                 "include")),
            _isConstant(basics::JsonHelper::checkAndGetBooleanValue(json.json(),
                                                             "isConstant")),
            _defined(false),
            _expressionAst(nullptr) {
          triagens::basics::Json bound = json.get("bound");

          if (! bound.isEmpty()) {
            _bound = bound;
            _defined = true;
          }
        }
      
        RangeInfoBound () 
          : _bound(), 
            _include(false), 
            _isConstant(false), 
            _defined(false),
            _expressionAst(nullptr) {
        }

        RangeInfoBound (RangeInfoBound const& copy) 
          : _bound(copy._bound.copy()), 
            _include(copy._include), 
            _isConstant(copy._isConstant), 
            _defined(copy._defined),
            _expressionAst(nullptr) {
        } 
          
////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RangeInfoBound () {}
      
////////////////////////////////////////////////////////////////////////////////
/// @brief delete assignment
////////////////////////////////////////////////////////////////////////////////

        RangeInfoBound& operator= (RangeInfoBound const& copy) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief explicit assign
////////////////////////////////////////////////////////////////////////////////

        void assign (basics::Json const& json) {
          _defined = false;   // keep it undefined in case of an exception
          triagens::basics::Json bound = json.get("bound");
          if (! bound.isEmpty()) {
            _bound = bound;
            _defined = true;
          }
          _include = basics::JsonHelper::checkAndGetBooleanValue(
                           json.json(), "include");
          _isConstant = basics::JsonHelper::checkAndGetBooleanValue(
                           json.json(), "isConstant");
          _expressionAst = nullptr;
        }

        void assign (AstNode const* bound, bool include) {
          _defined = false;  // keep it undefined in case of an exception
          _include = include;
          if (bound->type == NODE_TYPE_VALUE) {
            _bound = triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                            bound->toJsonValue(TRI_UNKNOWN_MEM_ZONE));
            _isConstant = true;
            _expressionAst = nullptr;
          }
          else {
            _bound = triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                            bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
            _isConstant = false;
            _expressionAst = bound;
          }
          _defined = true;
        }
      
        void assign (RangeInfoBound const& copy) {
          _defined = false;
          _bound = copy._bound.copy(); 
          _include = copy._include;
          _isConstant = copy._isConstant;
          _defined = copy._defined;
          _expressionAst = copy._expressionAst;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson() const {
          triagens::basics::Json item(basics::Json::Array, 3);
          if (! _bound.isEmpty()) {
            item("bound", _bound.copy());
          }
          item("include", triagens::basics::Json(_include))
              ("isConstant", triagens::basics::Json(!_defined || _isConstant));
          return item;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief toIndexOperator, doesn't work with unbounded above and below 
/// RangeInfos and only for constant values
////////////////////////////////////////////////////////////////////////////////

        TRI_index_operator_t* toIndexOperator (bool high, 
                                               triagens::basics::Json parameters,
                                               TRI_shaper_t* shaper) const {

          TRI_ASSERT(_isConstant);

          TRI_index_operator_type_e op;

          if (high) {
            if (_include) {
              op = TRI_LE_INDEX_OPERATOR;
            }
            else {
              op = TRI_LT_INDEX_OPERATOR;
            }
          } 
          else {
            if (_include) {
              op = TRI_GE_INDEX_OPERATOR;
            }
            else {
              op = TRI_GT_INDEX_OPERATOR;
            }
          }
          parameters.add(_bound.copy());
          size_t nr = parameters.size();

          return TRI_CreateIndexOperator(op, nullptr, nullptr, parameters.steal(), shaper, nr);
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineLowerBounds, changes the bound in *this and replaces
/// it by the stronger bound of *this and that, interpreting both as lower
/// bounds.
////////////////////////////////////////////////////////////////////////////////

        void andCombineLowerBounds (RangeInfoBound const& that);

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineUpperBounds, changes the bound in *this and replaces
/// it by the stronger bound of *this and that, interpreting both as upper
/// bounds.
////////////////////////////////////////////////////////////////////////////////

        void andCombineUpperBounds (RangeInfoBound const& that);

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for bound
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json const& bound() const {
          return _bound;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for inclusion
////////////////////////////////////////////////////////////////////////////////

        bool inclusive () const {
          return _include;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for isConstant
////////////////////////////////////////////////////////////////////////////////

        bool isConstant () const {
          return ! _defined || _isConstant;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for isDefined
////////////////////////////////////////////////////////////////////////////////

        bool isDefined () const {
          return _defined;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getExpressionAst, looks up or computes (if necessary) an AST
/// for the variable bound, return nullptr for a constant bound, the new
/// (if needed) nodes are registered with the ast
////////////////////////////////////////////////////////////////////////////////

        AstNode const* getExpressionAst (Ast* ast) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief _bound as Json, this is either for constant values 
/// (_isConstant==true) or for JSON-serialised subexpressions
/// (_isConstant==false).
////////////////////////////////////////////////////////////////////////////////

      private:

        triagens::basics::Json _bound;

////////////////////////////////////////////////////////////////////////////////
/// @brief _include, flag indicating whether or not bound is included
////////////////////////////////////////////////////////////////////////////////

        bool _include;

////////////////////////////////////////////////////////////////////////////////
/// @brief _isConstant, this is true if the bound is a constant value
////////////////////////////////////////////////////////////////////////////////

        bool _isConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief _defined, if this is true if the bound is defined
////////////////////////////////////////////////////////////////////////////////

        bool _defined;

////////////////////////////////////////////////////////////////////////////////
/// @brief _expressionAst, this remembers the AST for the expression
/// in the variable case, for constant expressions this is always a nullptr.
/// If the bound is made from Json, then _expressionAst is initially set
/// to nullptr and only later computed by getExpressionAst and then
/// cached. Note that the memory management is done by an object of type
/// Ast outside of this class. Therefore the destructor does not delete
/// the pointer here.
////////////////////////////////////////////////////////////////////////////////

        AstNode mutable const* _expressionAst;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep a list of RangeInfoBounds for _lows and one for 
///_highs, as well as constant _lowConst and _highConst. All constant bounds
/// will be combined into _lowConst and _highConst respectively, they
/// can also be not _defined. All bounds in _lows and _highs are defined
/// and not constant. The flag _defined is only false if the whole RangeInfo
/// is not defined (default constructor). 
/// This struct keeps _valid and _equality up to date at all times.
/// _valid is false iff the range is known to be empty. _equality is
/// true iff upper and lower bounds were given as (list of) identical
/// pairs. Note that _equality can be set and yet we only notice that
/// the RangeInfo is empty at runtime, if more than one equality was given!
////////////////////////////////////////////////////////////////////////////////
    
    struct RangeInfo {
        
////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        RangeInfo (std::string const& var,
                   std::string const& attr,
                   RangeInfoBound low, 
                   RangeInfoBound high,
                   bool equality)
          : _var(var), 
            _attr(attr), 
            _valid(true), 
            _defined(true),
            _equality(equality) {

          if (low.isConstant()) {
            _lowConst.assign(low);
          }
          else {
            _lows.emplace_back(low);
          }

          if (high.isConstant()) {
            _highConst.assign(high);
          }
          else {
            _highs.emplace_back(high);
          }

          // Maybe the range is known to be invalid right away?
          if (_lowConst.isDefined() && _lowConst.isConstant() &&
              _highConst.isDefined() && _highConst.isConstant()) {
            int cmp = TRI_CompareValuesJson(_lowConst.bound().json(),
                                            _highConst.bound().json(), true);
            if (cmp == 1) {
              _valid = false;
            }
            else if (cmp == 0) {
              if (_lowConst.inclusive() && _highConst.inclusive()) {
                _equality = true;
              }
              else {
                _valid = false;
              }
            }
          }
        }

        RangeInfo (std::string const& var,
                   std::string const& attr)
          : _var(var), 
            _attr(attr), 
            _valid(true), 
            _defined(true),
            _equality(false) {
        }

        RangeInfo () 
          : _valid(false), 
            _defined(false), 
            _equality(false) {
        }
        
        RangeInfo (basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
        
        ~RangeInfo () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief delete assignment operator
////////////////////////////////////////////////////////////////////////////////
        
        RangeInfo& operator= (RangeInfo const& copy) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////
        
        triagens::basics::Json toJson () const;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toString
////////////////////////////////////////////////////////////////////////////////
        
        std::string toString () const {
          return this->toJson().toString();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief is1ValueRangeInfo
////////////////////////////////////////////////////////////////////////////////
        
        // is the range a unique value (i.e. something like x<=1 and x>=1),
        // note again that with variable bounds it can turn out only at 
        // runTime that two or more expressions actually contradict each other.
        // In this case this method will return true nevertheless.
        bool is1ValueRangeInfo () const { 
          if (! _defined || ! _valid) {
            return false;
          }
          return _equality;
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief isDefined, getter for _defined
////////////////////////////////////////////////////////////////////////////////

        bool isDefined () const {
          return _defined;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief isValid, getter for _valid
////////////////////////////////////////////////////////////////////////////////

        bool isValid () const {
          return _valid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate
////////////////////////////////////////////////////////////////////////////////

        void invalidate () {
          _valid = false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief isConstant
////////////////////////////////////////////////////////////////////////////////

        bool isConstant () const {
          if (! _defined) {
            return false;
          }
          if (! _valid) {
            return true;
          }
          return _lows.size() == 0 && _highs.size() == 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief fuse, fuse two ranges, must be for the same variable and attribute
////////////////////////////////////////////////////////////////////////////////

        void fuse (RangeInfo const& that);

////////////////////////////////////////////////////////////////////////////////
/// @brief _var, the AQL variable name
////////////////////////////////////////////////////////////////////////////////
        
        std::string _var;

////////////////////////////////////////////////////////////////////////////////
/// @brief _attr, the attribute access in the variable, can have dots meaning
/// deep access
////////////////////////////////////////////////////////////////////////////////
        
        std::string _attr;

////////////////////////////////////////////////////////////////////////////////
/// @brief _lows, all non-constant lower bounds
////////////////////////////////////////////////////////////////////////////////
        
        std::list<RangeInfoBound> _lows;

////////////////////////////////////////////////////////////////////////////////
/// @brief _lowConst, all constant lower bounds combined, or not _defined
////////////////////////////////////////////////////////////////////////////////
        
        RangeInfoBound _lowConst;

////////////////////////////////////////////////////////////////////////////////
/// @brief _highs, all non-constant upper bounds
////////////////////////////////////////////////////////////////////////////////
        
        std::list<RangeInfoBound> _highs;

////////////////////////////////////////////////////////////////////////////////
/// @brief _highConst, all constant upper bounds combined, or not _defined
////////////////////////////////////////////////////////////////////////////////
        
        RangeInfoBound _highConst;

////////////////////////////////////////////////////////////////////////////////
/// @brief revokeEquality, this is used when we withdraw a variable bound,
/// in that case we can no longer trust the _equality bit, no big harm is
/// done, except that we no longer know that the range is only at most a
/// single value
////////////////////////////////////////////////////////////////////////////////

        void revokeEquality () {
          _equality = false;
        }

        RangeInfo clone () {
          //TODO improve this
          return RangeInfo(this->toJson());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief _valid, this is set to true iff the range is known to be non-empty
////////////////////////////////////////////////////////////////////////////////
        
      private:

        bool _valid;

////////////////////////////////////////////////////////////////////////////////
/// @brief _defined, this is set iff the range is defined
////////////////////////////////////////////////////////////////////////////////
        
        bool _defined;

////////////////////////////////////////////////////////////////////////////////
/// @brief _equality, range is known to be an equality
////////////////////////////////////////////////////////////////////////////////
        
        bool _equality;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief class to keep RangeInfos associated to variable and attribute names. 
////////////////////////////////////////////////////////////////////////////////
    
    class RangeInfoMap {
        
      public:
        
        RangeInfoMap (const RangeInfoMap& copy) = delete;
        RangeInfoMap& operator= (RangeInfoMap const& copy) = delete;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief default constructor
////////////////////////////////////////////////////////////////////////////////
    
        RangeInfoMap () : _ranges() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
    
        ~RangeInfoMap() {
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief find, find all the range infos for variable <var>,
/// ownership is not transferred
////////////////////////////////////////////////////////////////////////////////
    
        std::unordered_map<std::string, RangeInfo>* find (std::string const& var) {
          auto it = _ranges.find(var);
          if (it == _ranges.end()) {
            return nullptr;
          }
          return &((*it).second);
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief insert, insert if it's not already there and otherwise
/// intersection with existing range, for variable values we keep them all.
/// The equality flag should be set if and only if the caller knows that the
/// lower and upper bound are equal, this is particularly important if the
/// bounds are variable and can only be computed at runtime.
////////////////////////////////////////////////////////////////////////////////
    
        void insert (std::string const& var, 
                     std::string const& name, 
                     RangeInfoBound low, 
                     RangeInfoBound high,
                     bool equality);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert, directly using a RangeInfo structure
////////////////////////////////////////////////////////////////////////////////

        void insert (RangeInfo range);
        
////////////////////////////////////////////////////////////////////////////////
/// @brief size, the number of range infos stored
////////////////////////////////////////////////////////////////////////////////
    
        size_t size () const {
          return _ranges.size();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toString, via Json
////////////////////////////////////////////////////////////////////////////////
    
        std::string toString() const {
          return this->toJson().toString();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////
    
        triagens::basics::Json toJson() const {
          triagens::basics::Json list(triagens::basics::Json::List);
          for (auto x : _ranges) {
            for (auto y: x.second) {
              triagens::basics::Json item(triagens::basics::Json::Array);
              item("variable", triagens::basics::Json(x.first))
                  ("attribute name", triagens::basics::Json(y.first))
                  ("range info", y.second.toJson());
              list(item);
            }
          }
          return list;
        }
        
        RangeInfoMap* clone ();
        RangeInfoMap* cloneExcluding (std::string const&);
        void eraseEmptyOrUndefined (std::string const&);
        bool isValid ();

////////////////////////////////////////////////////////////////////////////////
/// @brief private data
////////////////////////////////////////////////////////////////////////////////
    
      private: 
        std::unordered_map<std::string, std::unordered_map<std::string, RangeInfo>> _ranges; 
        
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief class to keep a vector of rangeInfoMapVec associated to variable and
/// attribute names, which will be or-combined 
////////////////////////////////////////////////////////////////////////////////
    
    class RangeInfoMapVec {
        
      public:
        
        RangeInfoMapVec (const RangeInfoMapVec& copy) = delete;
        RangeInfoMapVec& operator= (RangeInfoMapVec const& copy) = delete;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief default constructor
////////////////////////////////////////////////////////////////////////////////
    
        RangeInfoMapVec () : _rangeInfoMapVec() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
    
        ~RangeInfoMapVec() {
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toString, via Json
////////////////////////////////////////////////////////////////////////////////
    
        std::string toString() const {
          return this->toJson().toString();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////
    
        triagens::basics::Json toJson() const {
          triagens::basics::Json list(triagens::basics::Json::List);
          for (size_t i = 0; i < _rangeInfoMapVec.size(); i++) {
            list(_rangeInfoMapVec[i]->toJson());
          }
          return list;
        }
      
        void eraseEmptyOrUndefined (std::string const&);
        void insertAnd (std::string const& var, 
                        std::string const& name, 
                        RangeInfoBound low, 
                        RangeInfoBound high,
                        bool equality);


        void insertAnd (RangeInfo range);
        
        void insertOr (std::vector<RangeInfo> ranges);

        void insertDistributeAndIntoOr (std::vector<RangeInfo> ranges);

        std::unordered_map<std::string, RangeInfo>* find (std::string const& var, size_t pos);

        std::vector<size_t> validPositions (); 
        // in what positions are the RangeInfoMaps in the vector valid

////////////////////////////////////////////////////////////////////////////////
/// @brief private data
////////////////////////////////////////////////////////////////////////////////
    
      private: 
        std::vector<RangeInfoMap*> _rangeInfoMapVec; 
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief IndexOrCondition, type for vector of vector of RangeInfo. The meaning
/// is: the outer vector means an implicit "OR" between the entries. Each
/// entry is a vector whose entries correspond to the attributes of the
/// index. They are a RangeInfo specifying the condition for that attribute.
/// Note that in the variable range bound case one RangeInfo can contain
/// multiple conditions which are implicitly "AND"ed.
////////////////////////////////////////////////////////////////////////////////
    
    typedef std::vector<std::vector<RangeInfo>> IndexOrCondition;
  }
}

#endif
