////////////////////////////////////////////////////////////////////////////////
/// @brief fundamental types for the optimisation and execution of AQL
///
/// @file arangod/Aql/Types.h
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

#ifndef ARANGODB_AQL_TYPES_H
#define ARANGODB_AQL_TYPES_H 1

#include <Basics/JsonHelper.h>

#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "V8/v8-conv.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"
#include "Utils/V8TransactionContext.h"
#include "Utils/AqlTransaction.h"


namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                            AqlDoc
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief type for register numbers/ids
////////////////////////////////////////////////////////////////////////////////

    typedef unsigned int RegisterId;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declaration for blocks of items
////////////////////////////////////////////////////////////////////////////////

    class AqlItemBlock;

////////////////////////////////////////////////////////////////////////////////
/// @brief a struct to hold a value, registers hole AqlValue* during the 
/// execution
////////////////////////////////////////////////////////////////////////////////

    struct AqlValue {

////////////////////////////////////////////////////////////////////////////////
/// @brief AqlValueType, indicates what sort of value we have
////////////////////////////////////////////////////////////////////////////////

      enum AqlValueType {
        EMPTY,     // contains no data
        JSON,      // TRI_json_t
        DOCUMENT,  // TRI_doc_mptr_t
        DOCVEC,    // a vector of blocks of results coming from a subquery
        RANGE      // a range just remembering lower and upper bound
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief Range, to hold a range compactly
////////////////////////////////////////////////////////////////////////////////

      struct Range {
        int64_t const _low;
        int64_t const _high;
        Range (int64_t low, int64_t high) : _low(low), _high(high) {}
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual data
////////////////////////////////////////////////////////////////////////////////

      union {
        triagens::basics::Json*     _json;
        TRI_df_marker_t const*      _shaped;
        std::vector<AqlItemBlock*>* _vector;
        Range const*                _range;
      };
      
////////////////////////////////////////////////////////////////////////////////
/// @brief _type, the type of value
////////////////////////////////////////////////////////////////////////////////

      AqlValueType _type;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors for the various value types, note that they all take
/// ownership of the corresponding pointers
////////////////////////////////////////////////////////////////////////////////

      AqlValue () : _type(EMPTY) {
      }

      AqlValue (triagens::basics::Json* json)
        : _json(json), _type(JSON) {
      }
      
      AqlValue (TRI_df_marker_t const* marker)
        : _marker(marker), _type(DOCUMENT) {
      }
      
      AqlValue (std::vector<AqlItemBlock*>* vector)
        : _vector(vector), _type(DOCVEC) {
      }

      AqlValue (int64_t low, int64_t high) 
        : _type(RANGE) {
        _range = new Range(low, high);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor, doing nothing automatically!
////////////////////////////////////////////////////////////////////////////////

      ~AqlValue () {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy, explicit destruction, only when needed
////////////////////////////////////////////////////////////////////////////////

      void destroy () {
        switch (_type) {
          case JSON: {
            if (_json != nullptr) {
              delete _json;
            }
            break;
          }
          case DOCVEC: {
            if (_vector != nullptr) {
              for (auto it = _vector->begin(); it != _vector->end(); ++it) {
                delete *it;
              }
              delete _vector;
            }
            delete _vector;
            break;
          }
          case RANGE: {
            delete _range;
            break;
          }
          case SHAPED: {
            // do nothing here, since data pointers need not be freed
            break;
          }
          default: {
            TRI_ASSERT(false);
          }
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone for recursive copying
////////////////////////////////////////////////////////////////////////////////

      AqlValue clone () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> toV8 (AQL_TRANSACTION_V8* trx, 
                                  TRI_document_collection_t const* document) const {
        switch (_type) {
          case JSON: {
            return TRI_ObjectJson(_json->json());
          }

          case DOCUMENT: {
            return TRI_WrapShapedJson(*trx, document->_info._cid, _marker);
          }

          case DOCVEC: {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
          }

          case RANGE: {
            v8::Handle<v8::Array> values = v8::Array::New();
            // TODO: fill range
            THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
            return values;
          }

          default: {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
          }
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief toString method
////////////////////////////////////////////////////////////////////////////////

      std::string toString (TRI_document_collection_t const* document) {
        switch (_type) {
          case JSON: {
            return _json->toString();
          }

          case SHAPED: {
            TRI_shaper_t* shaper = document->getShaper();
            TRI_shaped_json_t shaped;
            TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, _marker);
            triagens::basics::Json json(shaper->_memoryZone, TRI_JsonShapedJson(shaper, &shaped));

            char const* key = TRI_EXTRACT_MARKER_KEY(_marker);
            std::string id(document->_info._name);
            id.push_back('/');
            id += std::string(key);
            json("_id", triagens::basics::Json(id));
            json("_rev", triagens::basics::Json(std::to_string(TRI_EXTRACT_MARKER_RID))); 
            json("_key", triagens::basics::Json(key));
            
            return json.toString();
          }

          case DOCVEC: {
            std::stringstream s;
            s << "I am a DOCVEC with " << _vector->size() << " blocks.";
            return s.str();
          }

          case RANGE: {
            std::stringstream s;
            s << "I am a range: " << _range->_low << " .. " << _range->_high;
            return s.str();
          }

          default:
            return std::string("");
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is empty
////////////////////////////////////////////////////////////////////////////////

      bool isEmpty () {
        return _type == EMPTY;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is true
////////////////////////////////////////////////////////////////////////////////

      bool isTrue () {
        if (_type != JSON) {
          return false;
        }
        TRI_json_t* json = _json->json();
        if (TRI_IsBooleanJson(json) && json->_value._boolean) {
          return true;
        }
        else if (TRI_IsNumberJson(json) && json->_value._number != 0.0) {
          return true;
        }
        else if (TRI_IsStringJson(json) && json->_value._string.length != 0) {
          return true;
        }
        else {
          return false;
        }
      }
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlItemBlock
// -----------------------------------------------------------------------------

    class AqlItemBlock {

        std::vector<AqlValue> _data;
        size_t     _nrItems;
        RegisterId _nrRegs;

      public:

        AqlItemBlock (size_t nrItems, RegisterId nrRegs)
          : _nrItems(nrItems), _nrRegs(nrRegs) {
          if (nrItems > 0 && nrRegs > 0) {
            _data.reserve(nrItems * nrRegs);
            for (size_t i = 0; i < nrItems * nrRegs; i++) {
              _data.emplace_back();
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AqlItemBlock () {
          std::unordered_set<AqlValue> cache;
          for (i = 0; i < _nrItems * _nrRegs; i++) {
            if (! _data[i].isEmpty()) {
              auto it = cache.find(_data[i]);
              if (it == cache.end()) {
                cache.insert(_data[i]);
                _data[i].destroy();
              }
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getValue, get the value of a variable
////////////////////////////////////////////////////////////////////////////////

      AqlValue getValue (size_t index, RegisterId varNr) const {
        return _data[index * _nrRegs + varNr];
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief setValue, set the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

      void setValue (size_t index, RegisterId varNr, AqlValue zeug) {
        TRI_ASSERT(_data[index * _nrRegs + varNr].isEmpty());
        _data[index * _nrRegs + varNr] = zeug;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _nrRegs
////////////////////////////////////////////////////////////////////////////////

      RegisterId getNrRegs () const {
        return _nrRegs;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _nrItems
////////////////////////////////////////////////////////////////////////////////

      size_t size () const {
        return _nrItems;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _data
////////////////////////////////////////////////////////////////////////////////

      vector<AqlValue>& getData () const {
        return _data;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone
////////////////////////////////////////////////////////////////////////////////

      AqlItemBlock* slice (size_t from, size_t to) {
        TRI_ASSERT(from < to && to <= _nrItems);

        std::unordered_map<AqlValue, AqlValue> cache;
        auto res = new AqlItemBlock(to - from, _nrRegs);
        for (size_t row = from; row < to; row++) {
          for (RegisterId col = 0; col < _nrRegs; col++) {
            AqlValue& a(_data[row * _nrRegs + col]);

            if (! a.isEmpty()) {
              auto it = cache.find(a);
              if (it == cache.end()) {
                AqlValue b = a.clone();
                res->_data[(row - from) * _nrRegs + col] = b;
                cache.insert(make_pair(a,b));
              }
              else {
                res->_data[(row - from) * _nrRegs + col] = it->second;
              }
            }
          }
        }
        return res;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief slice/clone for a subset
////////////////////////////////////////////////////////////////////////////////

      AqlItemBlock* slice (vector<size_t>& chosen, size_t from, size_t to) {
        TRI_ASSERT(from < to && to <= chosen.size());

        std::unordered_map<AqlValue, AqlValue> cache;
        auto res = new AqlItemBlock(to - from, _nrRegs);
        for (size_t row = from; row < to; row++) {
          for (RegisterId col = 0; col < _nrRegs; col++) {
            AqlValue& a(_data[chosen[row] * _nrRegs + col]);

            if (! a.isEmpty()) {
              auto it = cache.find(a);
              if (it == cache.end()) {
                AqlValue b = a->clone();
                res->_data[(row - from) * _nrRegs + col] = b;
                cache.insert(make_pair(a,b));
              }
              else {
                res->_data[(row - from) * _nrRegs + col] = it->second;
              }
            }
          }
        }
        return res;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief splice multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

      static AqlItemBlock* splice(std::vector<AqlItemBlock*>& blocks);

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

