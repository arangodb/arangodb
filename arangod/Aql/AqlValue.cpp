////////////////////////////////////////////////////////////////////////////////
/// @brief AQL value
///
/// @file arangod/Aql/AqlValue.cpp
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

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "BasicsC/json-utilities.h"
#include "V8/v8-conv.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/voc-shaper.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is true
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isTrue () const {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy, explicit destruction, only when needed
////////////////////////////////////////////////////////////////////////////////

void AqlValue::destroy () {
  switch (_type) {
    case JSON: {
      if (_json != nullptr) {
        delete _json;
        _json = nullptr;
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
 
  // to avoid double freeing     
  _type = EMPTY;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone for recursive copying
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::clone () const {
  switch (_type) {
    case JSON: {
      return AqlValue(new Json(_json->copy()));
    }

    case SHAPED: {
      return AqlValue(_marker);
    }

    case DOCVEC: {
      auto c = new std::vector<AqlItemBlock*>;
      c->reserve(_vector->size());
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        c->push_back((*it)->slice(0, (*it)->size()));
      }
      return AqlValue(c);
    }

    case RANGE: {
      return AqlValue(_range->_low, _range->_high);
    }

    case EMPTY: {
      return AqlValue();
    }

    default: {
      TRI_ASSERT(false);
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> AqlValue::toV8 (AQL_TRANSACTION_V8* trx, 
                                      TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      return TRI_ObjectJson(_json->json());
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);
      return TRI_WrapShapedJson<AQL_TRANSACTION_V8>(*trx, document->_info._cid, _marker);
    }

    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result list length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result list
      v8::Handle<v8::Array> result = v8::Array::New(static_cast<int>(totalSize));
      uint32_t j = 0; // output row count

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);
        for (size_t i = 0; i < n; ++i) {
          result->Set(j++, current->getValue(i, 0).toV8(trx, vecCollection));
        }
      }
      return result;
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      int64_t const n = _range->_high - _range->_low + 1;
      v8::Handle<v8::Array> result = v8::Array::New(static_cast<int>(n));
      
      uint32_t j = 0; // output row count
      for (int64_t i = _range->_low; i <= _range->_high; ++i) {
        // is it safe to use a double here (precision loss)?
        result->Set(j++, v8::Number::New(static_cast<double>(i)));
      }

      return result;
    }

    case EMPTY: {
      return v8::Object::New(); // TODO?
    }
  }
      
  // should never get here
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toString method
////////////////////////////////////////////////////////////////////////////////

std::string AqlValue::toString (TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      return _json->toString();
    }

    case SHAPED: {
      // we're lazy and just stringify the json representation
      // this does not matter as this code is not performance-sensitive
      return toJson(document).toString();
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

    case EMPTY: {
      return std::string("empty");
    }
  }
      
  // should never get here
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson method
////////////////////////////////////////////////////////////////////////////////
      
Json AqlValue::toJson (TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      return _json->copy();
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      TRI_shaper_t* shaper = document->getShaper();
      TRI_shaped_json_t shaped;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, _marker);
      triagens::basics::Json json(shaper->_memoryZone, TRI_JsonShapedJson(shaper, &shaped));

      char const* key = TRI_EXTRACT_MARKER_KEY(_marker);
      // TODO: use CollectionNameResolver
      std::string id(document->_info._name);
      id.push_back('/');
      id += std::string(key);
      json("_id", triagens::basics::Json(id));
      json("_rev", triagens::basics::Json(std::to_string(TRI_EXTRACT_MARKER_RID(_marker) )));
      json("_key", triagens::basics::Json(key));

      // TODO: return _from and _to, and fix order of attributes!
      return json;
    }
          
    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result list length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result list
      Json json(Json::List, static_cast<size_t>(totalSize));

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);
        for (size_t i = 0; i < n; ++i) {
          json.add(current->getValue(i, 0).toJson(vecCollection));
        }
      }

      return json;
    }
          
    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      int64_t const n = _range->_high - _range->_low + 1;
      Json json(Json::List, static_cast<size_t>(n));

      for (int64_t i = _range->_low; i <= _range->_high; ++i) {
        // is it safe to use a double here (precision loss)?
        json.add(Json(static_cast<double>(i)));
      }

      return json;
    }

    case EMPTY: {
      return triagens::basics::Json();
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::CreateFromBlocks (std::vector<AqlItemBlock*> const& src,
                                     std::vector<std::string> const& variableNames) {
  size_t totalSize = 0;

  for (auto it = src.begin(); it != src.end(); ++it) {
    totalSize += (*it)->size();
  }

  auto json = new Json(Json::List, totalSize);

  try {
    for (auto it = src.begin(); it != src.end(); ++it) {
      auto current = (*it);
      RegisterId const n = current->getNrRegs();

      for (size_t i = 0; i < current->size(); ++i) {
        Json values(Json::Array);

        for (RegisterId j = 0; j < n; ++j) {
          if (variableNames[j][0] != '\0') {
            // temporaries don't have a name and won't be included
            values.set(variableNames[j].c_str(), current->getValue(i, j).toJson(current->getDocumentCollection(j)));
          }
        }

        json->add(values);
      }
    }

    return AqlValue(json);
  }
  catch (...) {
    delete json;
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

int AqlValue::Compare (AqlValue const& left,  
                       TRI_document_collection_t const* leftcoll,
                       AqlValue const& right, 
                       TRI_document_collection_t const* rightcoll) {
  if (left._type != right._type) {
    if (left._type == AqlValue::EMPTY) {
      return -1;
    }

    if (right._type == AqlValue::EMPTY) {
      return 1;
    }

    if (left._type == AqlValue::JSON && right._type == AqlValue::SHAPED) {
      triagens::basics::Json rjson = right.toJson(rightcoll);
      return TRI_CompareValuesJson(left._json->json(), rjson.json());
    }

    if (left._type == AqlValue::SHAPED && right._type == AqlValue::JSON) {
      triagens::basics::Json ljson = left.toJson(leftcoll);
      return TRI_CompareValuesJson(ljson.json(), right._json->json());
    }

    // No other comparisons are defined
    TRI_ASSERT(false);
  }

  // if we get here, types are equal

  switch (left._type) {
    case AqlValue::EMPTY: {
      return 0;
    }

    case AqlValue::JSON: {
      return TRI_CompareValuesJson(left._json->json(), right._json->json());
    }

    case AqlValue::SHAPED: {
      TRI_shaped_json_t l;
      TRI_shaped_json_t r;
      TRI_EXTRACT_SHAPED_JSON_MARKER(l, left._marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(r, right._marker);
                
      return TRI_CompareShapeTypes(nullptr, nullptr, &l, leftcoll->getShaper(), 
                                   nullptr, nullptr, &r, rightcoll->getShaper());
    }

    case AqlValue::DOCVEC: { 
      // use lexicographic ordering of AqlValues regardless of block,
      // DOCVECs have a single register coming from ReturnNode.
      size_t lblock = 0;
      size_t litem = 0;
      size_t rblock = 0;
      size_t ritem = 0;
      
      while (lblock < left._vector->size() && 
             rblock < right._vector->size()) {
        AqlValue lval = left._vector->at(lblock)->getValue(litem, 0);
        AqlValue rval = right._vector->at(rblock)->getValue(ritem, 0);
        int cmp = Compare(lval, 
                          left._vector->at(lblock)->getDocumentCollection(0),
                          rval, 
                          right._vector->at(rblock)->getDocumentCollection(0));
        if (cmp != 0) {
          return cmp;
        }
        if (++litem == left._vector->size()) {
          litem = 0;
          lblock++;
        }
        if (++ritem == right._vector->size()) {
          ritem = 0;
          rblock++;
        }
      }

      if (lblock == left._vector->size() && 
          rblock == right._vector->size()){
        return 0;
      }

      return (lblock < left._vector->size() ? -1 : 1);
    }
    case AqlValue::RANGE: {
      if (left._range->_low < right._range->_low) {
        return -1;
      } 
      if (left._range->_low > right._range->_low) {
        return 1;
      } 
      if (left._range->_high < left._range->_high) {
        return -1;
      } 
      if (left._range->_high > left._range->_high) {
        return 1;
      }
      return 0;
    }

    default: {
      TRI_ASSERT(false);
      return 0;
    }
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

