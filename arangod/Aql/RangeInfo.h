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

#include "lib/Basics/json-utilities.h"

using Json = triagens::basics::Json;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                              class RangeInfoBound
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep an upper or lower bound named _bound and a bool
/// _include which indicates if the _bound is included or not. This can
/// hold a constant value (NODE_TYPE_VALUE) or a subexpression, which is
/// indicated by the boolean flag _isConstant.
////////////////////////////////////////////////////////////////////////////////

    struct RangeInfoBound {

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        RangeInfoBound (AstNode const* bound, bool include) 
          : _bound(), _include(include), _defined(false) {
          if (bound->type == NODE_TYPE_VALUE) {
            _bound = Json(TRI_UNKNOWN_MEM_ZONE,
                          bound->toJsonValue(TRI_UNKNOWN_MEM_ZONE));
            _isConstant = true;
          }
          else {
            _bound = Json(TRI_UNKNOWN_MEM_ZONE,
                          bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
            _isConstant = false;
          }
          _defined = true;
        }
        
        RangeInfoBound (basics::Json const& json) 
          : _bound(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE,
              basics::JsonHelper::checkAndGetArrayValue(json.json(), "bound"))),
            _include(basics::JsonHelper::checkAndGetBooleanValue(json.json(),
                                                                 "include")),
            _defined(true) {
        }
      
        RangeInfoBound () 
          : _bound(), _include(false), _isConstant(false), _defined(false) {
        }

        RangeInfoBound ( RangeInfoBound const& copy ) 
            : _bound(copy._bound.copy()), _include(copy._include), 
              _isConstant(copy._isConstant), _defined(copy._defined) {
        } 
          
////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RangeInfoBound () {}
      
////////////////////////////////////////////////////////////////////////////////
/// @brief delete assignment
////////////////////////////////////////////////////////////////////////////////

        RangeInfoBound& operator= ( RangeInfoBound const& copy ) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief explicit assign
////////////////////////////////////////////////////////////////////////////////

        void assign (basics::Json const& json) {
          _defined = false;   // keep it undefined in case of an exception
          _bound = Json(TRI_UNKNOWN_MEM_ZONE,
            basics::JsonHelper::checkAndGetArrayValue(json.json(), "bound"), 
            basics::Json::NOFREE).copy();
          _include = basics::JsonHelper::checkAndGetBooleanValue(
                           json.json(), "include");
          _isConstant = basics::JsonHelper::checkAndGetBooleanValue(
                           json.json(), "isConstant");
          _defined = true;
        }

        void assign (AstNode const* bound, bool include) {
          _defined = false;  // keep it undefined in case of an exception
          _include = include;
          if (bound->type == NODE_TYPE_VALUE) {
            _bound = Json(TRI_UNKNOWN_MEM_ZONE,
                          bound->toJsonValue(TRI_UNKNOWN_MEM_ZONE));
            _isConstant = true;
          }
          else {
            _bound = Json(TRI_UNKNOWN_MEM_ZONE,
                          bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
            _isConstant = false;
          }
          _defined = true;
        }
      
        void assign (RangeInfoBound& copy) {
          _defined = false;
          _bound = copy._bound.copy(); 
          _include = copy._include;
          _isConstant = copy._isConstant;
          _defined = copy._defined;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

        Json toJson () const {
          Json item(basics::Json::Array, 3);
          item("bound", _bound.copy())
              ("include", Json(_include))
              ("isConstant", Json(_isConstant));
          return item;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief toIndexOperator, doesn't work with unbounded above and below 
/// RangeInfos and only for constant values
////////////////////////////////////////////////////////////////////////////////

        TRI_index_operator_t* toIndexOperator (bool high, Json parameters,
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
          return TRI_CreateIndexOperator(op, NULL, NULL, parameters.steal(),
              shaper, NULL, nr, NULL);
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for bound
////////////////////////////////////////////////////////////////////////////////

        Json const& bound () const {
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
          return _isConstant;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for isDefined
////////////////////////////////////////////////////////////////////////////////

        bool isDefined () const {
          return _defined;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief _bound as Json, this is either for constant values 
/// (_isConstant==true) or for JSON-serialised subexpressions
/// (_isConstant==false).
////////////////////////////////////////////////////////////////////////////////

      private:

        Json _bound;

////////////////////////////////////////////////////////////////////////////////
/// @brief _include, flag indicating whether or not bound is included
////////////////////////////////////////////////////////////////////////////////

        bool _include;

////////////////////////////////////////////////////////////////////////////////
/// @brief _isConstant, this is true iff the bound is a constant value
////////////////////////////////////////////////////////////////////////////////

        bool _isConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief _defined, if this is true iff the bound is defined
////////////////////////////////////////////////////////////////////////////////

        bool _defined;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep a list of RangeInfoBounds for _low and one for _high, 
/// and _valid to indicate if the range is valid (i.e. not known to be
/// empty), the two vectors are always of length at least 1, if necessary,
/// they contain a single, undefined bound, in the case of all constant
/// bounds the vectors are kept at length exactly 1 by combining the
/// bounds, the constant bounds are always combined into the first one,
/// all non-constant ones are kept afterwards. 
////////////////////////////////////////////////////////////////////////////////
    
    struct RangeInfo {
        
////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        RangeInfo ( std::string var,
                    std::string attr,
                    RangeInfoBound low, 
                    RangeInfoBound high,
                    bool equality)
          : _var(var), _attr(attr), _valid(true), _defined(true),
            _equality(equality) {
          _low.emplace_back(low);
          _high.emplace_back(high);
        }

        RangeInfo ( std::string var,
                    std::string attr)
          : _var(var), _attr(attr), _valid(true), _defined(true),
            _equality(false) {
        }

        RangeInfo () : _valid(false), _defined(false), _equality(false) {
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
        
        RangeInfo& operator= ( RangeInfo const& copy ) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////
        
        Json toJson ();
        
////////////////////////////////////////////////////////////////////////////////
/// @brief toString
////////////////////////////////////////////////////////////////////////////////
        
        std::string toString () {
          return this->toJson().toString();
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief is1ValueRangeInfo
////////////////////////////////////////////////////////////////////////////////
        
        // is the range a unique value (i.e. something like x<=1 and x>=1)
        bool is1ValueRangeInfo () { 
          if (! _valid) {
            return false;
          }
          if (_equality) {
            return true;
          }
          // Detect an equality even if it was not given as one:
          bool res = _valid && _low.size() == 1 && _high.size() == 1 &&
                     _low[0].isDefined() && _high[0].isDefined() &&
                     _low[0].isConstant() && _high[0].isConstant();
          if (res == false) {
            return false;
          }
          res = TRI_CheckSameValueJson(_low[0].bound().json(),
                                       _high[0].bound().json()) &&
                _low[0].inclusive() && _high[0].inclusive();
          if (res == true) {
            _equality = true;
          }
          return res;
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
/// @brief _var, the AQL variable name
////////////////////////////////////////////////////////////////////////////////
        
        std::string _var;

////////////////////////////////////////////////////////////////////////////////
/// @brief _attr, the attribute access in the variable, can have dots meaning
/// deep access
////////////////////////////////////////////////////////////////////////////////
        
        std::string _attr;

////////////////////////////////////////////////////////////////////////////////
/// @brief _low, all lower bounds, constant lower bounds are combined into
/// the first one (index 0)
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<RangeInfoBound> _low;

////////////////////////////////////////////////////////////////////////////////
/// @brief _high, all upper bounds, constant upper bounds are combined into
/// the first one (index 0)
////////////////////////////////////////////////////////////////////////////////
        
        std::vector<RangeInfoBound> _high;

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
    
    class RangesInfo {
        
      public:
        
        RangesInfo( const RangesInfo& copy ) = delete;
        RangesInfo& operator= ( RangesInfo const& copy ) = delete;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief default constructor
////////////////////////////////////////////////////////////////////////////////
    
        RangesInfo () : _ranges() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
    
        ~RangesInfo() {
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief find, find the all the range infos for variable <var>,
/// ownership is not transferred
////////////////////////////////////////////////////////////////////////////////
    
        std::unordered_map<std::string, RangeInfo>* find (std::string var) {
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
    
        void insert (std::string var, std::string name, 
                     RangeInfoBound low, RangeInfoBound high,
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
    
        Json toJson () const {
          Json list(Json::List);
          for (auto x : _ranges) {
            for (auto y: x.second) {
              Json item(Json::Array);
              item("variable", Json(x.first))
                  ("attribute name", Json(y.first))
                  ("range info", y.second.toJson());
              list(item);
            }
          }
          return list;
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief private data
////////////////////////////////////////////////////////////////////////////////
    
      private: 
        std::unordered_map<std::string, std::unordered_map<std::string, RangeInfo>> _ranges; 
        
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief RangeInfoVec, type for vector of vector of RangeInfo. The meaning
/// is: the outer vector means an implicit "OR" between the entries. Each
/// entry is a vector which means an implicit "AND" between the individual
/// RangeInfo objects. This is actually the disjunctive normal form of
/// all conditions, therefore, every boolean expression of ANDs and ORs
/// can be expressed in this form.
////////////////////////////////////////////////////////////////////////////////
    
    typedef std::vector<std::vector<RangeInfo>> RangeInfoVec;
  }
}

#endif
