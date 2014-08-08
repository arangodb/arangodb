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

#include <functional>

#include <Basics/JsonHelper.h>
#include <BasicsC/json-utilities.h>

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
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief type for register numbers/ids
////////////////////////////////////////////////////////////////////////////////

    typedef unsigned int RegisterId;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declaration for blocks of items
////////////////////////////////////////////////////////////////////////////////

    class AqlItemBlock;

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct AqlValue
// -----------------------------------------------------------------------------

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
        JSON,      // Json*
        SHAPED,    // TRI_df_marker_t*
        DOCVEC,    // a vector of blocks of results coming from a subquery
        RANGE      // a pointer to a range remembering lower and upper bound
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
        TRI_df_marker_t const*      _marker;
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

      AqlValue () : _json(nullptr), _type(EMPTY) {
      }

      AqlValue (triagens::basics::Json* json)
        : _json(json), _type(JSON) {
      }
      
      AqlValue (TRI_df_marker_t const* marker)
        : _marker(marker), _type(SHAPED) {
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

      void destroy ();

////////////////////////////////////////////////////////////////////////////////
/// @brief erase, this does not free the stuff in the AqlValue, it only
/// erases the pointers and makes the AqlValue structure EMPTY, this
/// is used when the AqlValue is stolen and stored in another object
////////////////////////////////////////////////////////////////////////////////

      void erase () {
        _type = EMPTY;
        _json = nullptr;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone for recursive copying
////////////////////////////////////////////////////////////////////////////////

      AqlValue clone () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> toV8 (AQL_TRANSACTION_V8* trx, 
                                  TRI_document_collection_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief toString method
////////////////////////////////////////////////////////////////////////////////

      std::string toString (TRI_document_collection_t const*) const;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief toJson method
////////////////////////////////////////////////////////////////////////////////
      
      triagens::basics::Json toJson (TRI_document_collection_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

      static AqlValue createFromBlocks (std::vector<AqlItemBlock*> const&,
                                        std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is empty
////////////////////////////////////////////////////////////////////////////////

      inline bool isEmpty () const {
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

    int CompareAqlValues ( AqlValue const& left,  
                           TRI_document_collection_t const* leftcoll,
                           AqlValue const& right, 
                           TRI_document_collection_t const* rightcoll );
 
  } //closes namespace triagens::aql
}   //closes namespace triagens

////////////////////////////////////////////////////////////////////////////////
/// @brief hash function for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

namespace std {

  template<> struct hash<triagens::aql::AqlValue> {
    size_t operator () (triagens::aql::AqlValue const& x) const {
      std::hash<uint32_t> intHash;
      std::hash<void const*> ptrHash;
      size_t res = intHash(static_cast<uint32_t>(x._type));
      switch (x._type) {
        case triagens::aql::AqlValue::EMPTY: {
          return res;
        }
        case triagens::aql::AqlValue::JSON: {
          return res ^ ptrHash(x._json);
        }
        case triagens::aql::AqlValue::SHAPED: {
          return res ^ ptrHash(x._marker);
        }
        case triagens::aql::AqlValue::DOCVEC: {
          return res ^ ptrHash(x._vector);
        }
        case triagens::aql::AqlValue::RANGE: {
          return res ^ ptrHash(x._range);
        }
        default: {
          TRI_ASSERT(false);
          return 0;
        }
      }
    }
  };

  template<> struct equal_to<triagens::aql::AqlValue> {
    bool operator () (triagens::aql::AqlValue const& a,
                      triagens::aql::AqlValue const& b) const {
      if (a._type != b._type) {
        return false;
      }
      switch (a._type) {
        case triagens::aql::AqlValue::JSON: {
          return a._json == b._json;
        }
        case triagens::aql::AqlValue::SHAPED: {
          return a._marker == b._marker;
        }
        case triagens::aql::AqlValue::DOCVEC: {
          return a._vector == b._vector;
        }
        case triagens::aql::AqlValue::RANGE: {
          return a._range == b._range;
        }
        default: {
          TRI_ASSERT(false);
          return true;
        }
      }
    }
  };

} //closes namespace std

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

