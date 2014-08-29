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
/// _include which indicates if the _bound is included or not.
////////////////////////////////////////////////////////////////////////////////

    struct RangeInfoBound {

      RangeInfoBound (AstNode const* bound, bool include) 
        : _include(include), _undefined(false){
        _bound = Json(TRI_UNKNOWN_MEM_ZONE, bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
      }
      
      RangeInfoBound (basics::Json const& json) 
          :   _bound(Json(TRI_UNKNOWN_MEM_ZONE,
              basics::JsonHelper::checkAndGetArrayValue(json.json(), "bound"), 
              basics::Json::NOFREE).copy()),
        _include(basics::JsonHelper::checkAndGetBooleanValue(json.json(), "include")),
              _undefined(false) {}
      
      RangeInfoBound () : _include(false), _undefined(true) {}

      ~RangeInfoBound(){}
      
      RangeInfoBound ( RangeInfoBound const& copy ) 
          : _bound(copy._bound.copy()), _include(copy._include),
          _undefined(copy._undefined) {} 
          
      RangeInfoBound& operator= ( RangeInfoBound const& copy ) = delete;

      void assign ( basics::Json const& json ) {
        _bound = Json(TRI_UNKNOWN_MEM_ZONE,
              basics::JsonHelper::checkAndGetArrayValue(json.json(), "bound"), 
              basics::Json::NOFREE).copy();
        _include = basics::JsonHelper::checkAndGetBooleanValue(json.json(), "include");
        _undefined = false;
      }

      void assign (AstNode const* bound, bool include) {
        _include = include;
        _bound = Json(TRI_UNKNOWN_MEM_ZONE, bound->toJson(TRI_UNKNOWN_MEM_ZONE, true));
      }
      
      void assign (RangeInfoBound copy) {
        _include = copy._include;
        _bound = copy._bound; 
      }

      Json toJson () const {
        Json item(basics::Json::Array);
          item("bound", Json(TRI_UNKNOWN_MEM_ZONE, 
                TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, _bound.json())))
              ("include", Json(_include));
        return item;
      }
      
      TRI_index_operator_t* toIndexOperator (bool high, Json parameters,
          TRI_shaper_t* shaper) const {
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
        parameters.add(_bound.get("value").copy());
        size_t nr = parameters.size();
        return TRI_CreateIndexOperator(op, NULL, NULL, parameters.steal(),
            shaper, NULL, nr, NULL);
      } 

      Json _bound;
      bool _include;
      bool _undefined;

    };

////////////////////////////////////////////////////////////////////////////////
/// @brief struct to keep a pair of RangeInfoBounds _low and _high, and _valid
/// to indicate if the range is valid (i.e. not of the form x<2 and x>3).
////////////////////////////////////////////////////////////////////////////////
    
    struct RangeInfo{
        
        RangeInfo ( std::string var,
                    std::string attr,
                    RangeInfoBound low, 
                    RangeInfoBound high )
          : _var(var), _attr(attr), _low(low), _high(high), _valid(true), _undefined(false) {
        }

        RangeInfo ( std::string var,
                    std::string attr)
          : _var(var), _attr(attr), _valid(true), _undefined(false) {
        }

        RangeInfo () :  _valid(false), _undefined(true) {}
        
        RangeInfo (basics::Json const& json) :
          _var(basics::JsonHelper::checkAndGetStringValue(json.json(), "var")),
          _attr(basics::JsonHelper::checkAndGetStringValue(json.json(), "attr")),
          _valid(basics::JsonHelper::checkAndGetBooleanValue(json.json(), "valid")),
          _undefined(false) {

          if (! json.get("low").isEmpty()) {
            _low.assign(json.get("low"));
          }
          
          if (! json.get("high").isEmpty()) {
            _high.assign(json.get("high"));
          }
        }

        /* RangeInfo( RangeInfo const& copy ) 
            : _var(copy._var), _attr(copy._attr), _low(copy._low),
              _high(copy._high), _valid(copy._valid){} */
        
        RangeInfo& operator= ( RangeInfo const& copy ) = delete;

        ~RangeInfo(){}

        Json toJson () {
          Json item(basics::Json::Array);
          item("var", Json(_var))("attr", Json(_attr));
          if(!_low._undefined){
            item("low", _low.toJson());
          }
          if(!_high._undefined){
            item("high", _high.toJson());
          }
          item("valid", Json(_valid));
          return item;
        }
        
        std::string toString() {
          return this->toJson().toString();
        }
        
        // is the range a unique value (i.e. something like x<=1 and x>=1)
        bool is1ValueRangeInfo () { 
          return _valid && !_low._undefined  && !_high._undefined &&
                  TRI_CheckSameValueJson(_low._bound.json(), _high._bound.json())
                  && _low._include && _high._include;
        }
        
        std::string _var;
        std::string _attr;
        RangeInfoBound _low;
        RangeInfoBound _high;
        bool _valid;
        bool _undefined;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief class to keep RangeInfos associated to variable and attribute names. 
////////////////////////////////////////////////////////////////////////////////
    
    class RangesInfo{
        
      public:
        
        RangesInfo( const RangesInfo& copy ) = delete;
        RangesInfo& operator= ( RangesInfo const& copy ) = delete;
        
        RangesInfo () : _ranges(){}

        ~RangesInfo(){}
        
        // find the range info for variable <var> and attributes <name>
        RangeInfo find (std::string var, std::string name) const {
          auto it1 = _ranges.find(var);
          if (it1 == _ranges.end()) {
            return RangeInfo();
          }
          auto it2 = it1->second.find(name);
          if (it2 == it1->second.end()) {
            return RangeInfo();
          }
          return (*it2).second;
        }
        
        // find the all the range infos for variable <var>
        std::unordered_map<std::string, RangeInfo> find (std::string var) {
          auto it = _ranges.find(var);
          if (it == _ranges.end()) {
            return unordered_map<std::string, RangeInfo>();
          }
          return (*it).second;
        }
        
        // insert if it's not already there and otherwise intersection with
        // existing range
        void insert (RangeInfo range);
        
        void insert (std::string var, std::string name, 
                     RangeInfoBound low, RangeInfoBound high);

        // the number of range infos stored
        size_t size () const {
          return _ranges.size();
        }
        
        std::string toString() const {
          return this->toJson().toString();
        }
        
        Json toJson () const {
          Json list(Json::List);
          for (auto x : _ranges) {
            for (auto y: x.second){
              Json item(Json::Array);
              item("var", Json(x.first))
                  ("attribute name", Json(y.first))
                  ("range info", y.second.toJson());
              list(item);
            }
          }
          return list;
        }
        
      private: 
        std::unordered_map<std::string, std::unordered_map<std::string, RangeInfo>> _ranges; 
        
    };

    typedef std::vector<std::vector<RangeInfo>> RangeInfoVec;
  }
}

#endif
