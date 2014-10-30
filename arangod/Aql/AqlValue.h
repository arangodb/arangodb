////////////////////////////////////////////////////////////////////////////////
/// @brief fundamental types for the optimisation and execution of AQL
///
/// @file arangod/Aql/AqlValue.h
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

#ifndef ARANGODB_AQL_AQL_VALUE_H
#define ARANGODB_AQL_AQL_VALUE_H 1

#include "Basics/Common.h"
#include "Aql/Range.h"
#include "Basics/JsonHelper.h"
#include "Utils/V8TransactionContext.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

namespace triagens {
  namespace aql {

    class AqlItemBlock;

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct AqlValue
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a struct to hold a value, registers hole AqlValue* during the 
/// execution
////////////////////////////////////////////////////////////////////////////////

    struct AqlValue {

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors for the various value types, note that they all take
/// ownership of the corresponding pointers
////////////////////////////////////////////////////////////////////////////////

      AqlValue () 
        : _json(nullptr), 
          _type(EMPTY) {
      }

      explicit AqlValue (triagens::basics::Json* json)
        : _json(json), 
          _type(JSON) {
      }
      
      explicit AqlValue (TRI_df_marker_t const* marker)
        : _marker(marker), 
          _type(SHAPED) {
      }
      
      explicit AqlValue (std::vector<AqlItemBlock*>* vector)
        : _vector(vector), 
          _type(DOCVEC) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is empty
////////////////////////////////////////////////////////////////////////////////

      inline bool isEmpty () const {
        return _type == EMPTY;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is true
////////////////////////////////////////////////////////////////////////////////

      bool isTrue () const;

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
/// @brief destroy, explicit destruction, only when needed
////////////////////////////////////////////////////////////////////////////////

      void destroy ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of an AqlValue type
////////////////////////////////////////////////////////////////////////////////
      
      std::string getTypeString () const; 

////////////////////////////////////////////////////////////////////////////////
/// @brief clone for recursive copying
////////////////////////////////////////////////////////////////////////////////

      AqlValue clone () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a string value
////////////////////////////////////////////////////////////////////////////////

      bool isString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a numeric value
////////////////////////////////////////////////////////////////////////////////

      bool isNumber () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a boolean value
////////////////////////////////////////////////////////////////////////////////

      bool isBoolean () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a list value
////////////////////////////////////////////////////////////////////////////////

      bool isList () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains an array value
////////////////////////////////////////////////////////////////////////////////

      bool isArray () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of an AqlValue containing a list
////////////////////////////////////////////////////////////////////////////////

      size_t listSize () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of an AqlValue
////////////////////////////////////////////////////////////////////////////////

      int64_t toInt64 () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get a string representation of the AqlValue
/// this will fail if the value is not a string
////////////////////////////////////////////////////////////////////////////////

      std::string toString () const;


////////////////////////////////////////////////////////////////////////////////
/// @brief get a string representation of the AqlValue
/// this will fail if the value is not a string
////////////////////////////////////////////////////////////////////////////////

      char const* toChar () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> toV8 (triagens::arango::AqlTransaction*, 
                                  TRI_document_collection_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson method
////////////////////////////////////////////////////////////////////////////////
      
      triagens::basics::Json toJson (triagens::arango::AqlTransaction*,
                                     TRI_document_collection_t const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an attribute value from the AqlValue 
/// this will return null if the value is not an array
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json extractArrayMember (triagens::arango::AqlTransaction*,
                                                 TRI_document_collection_t const*,
                                                 char const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a value from a list AqlValue 
/// this will return null if the value is not a list
/// depending on the last parameter, the return value will either contain a
/// copy of the original value in the list or a reference to it (which must
/// not be freed)
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json extractListMember (triagens::arango::AqlTransaction*,
                                                TRI_document_collection_t const*,
                                                int64_t,
                                                bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

      static AqlValue CreateFromBlocks (triagens::arango::AqlTransaction*,
                                        std::vector<AqlItemBlock*> const&,
                                        std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison for AqlValue objects
////////////////////////////////////////////////////////////////////////////////
    
      static int Compare (triagens::arango::AqlTransaction*,
                          AqlValue const&,  
                          TRI_document_collection_t const*,
                          AqlValue const&, 
                          TRI_document_collection_t const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

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

    };

  } //closes namespace triagens::aql
}   //closes namespace triagens

// -----------------------------------------------------------------------------
// --SECTION--                                hash function helpers for AqlValue
// -----------------------------------------------------------------------------

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
        // case triagens::aql::AqlValue::EMPTY intentionally not handled here!
        // (should fall through and fail!)

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

