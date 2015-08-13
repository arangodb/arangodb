////////////////////////////////////////////////////////////////////////////////
/// @brief index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Index.h"
#include "Basics/Exceptions.h"
#include "Basics/json-utilities.h"
#include "VocBase/server.h"
#include "VocBase/VocShaper.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Index
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

Index::Index (TRI_idx_iid_t iid,
              TRI_document_collection_t* collection,
              std::vector<std::vector<triagens::basics::AttributeName>> const& fields) 
  : _iid(iid),
    _collection(collection),
    _fields(fields) {
}

Index::~Index () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type based on a type name
////////////////////////////////////////////////////////////////////////////////

Index::IndexType Index::type (char const* type) {
  if (::strcmp(type, "primary") == 0) {
    return TRI_IDX_TYPE_PRIMARY_INDEX;
  }
  if (::strcmp(type, "edge") == 0) {
    return TRI_IDX_TYPE_EDGE_INDEX;
  }
  if (::strcmp(type, "hash") == 0) {
    return TRI_IDX_TYPE_HASH_INDEX;
  }
  if (::strcmp(type, "skiplist") == 0) {
    return TRI_IDX_TYPE_SKIPLIST_INDEX;
  }
  if (::strcmp(type, "fulltext") == 0) {
    return TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  if (::strcmp(type, "cap") == 0) {
    return TRI_IDX_TYPE_CAP_CONSTRAINT;
  }
  if (::strcmp(type, "geo1") == 0) {
    return TRI_IDX_TYPE_GEO1_INDEX;
  }
  if (::strcmp(type, "geo2") == 0) {
    return TRI_IDX_TYPE_GEO2_INDEX;
  }

  return TRI_IDX_TYPE_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an index type
////////////////////////////////////////////////////////////////////////////////

char const* Index::typeName (Index::IndexType type) {
  switch (type) {
    case TRI_IDX_TYPE_PRIMARY_INDEX:
      return "primary";
    case TRI_IDX_TYPE_EDGE_INDEX:
      return "edge";
    case TRI_IDX_TYPE_HASH_INDEX:
      return "hash";
    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      return "skiplist";
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      return "fulltext";
    case TRI_IDX_TYPE_CAP_CONSTRAINT:
      return "cap";
    case TRI_IDX_TYPE_GEO1_INDEX:
      return "geo1";
    case TRI_IDX_TYPE_GEO2_INDEX:
      return "geo2";
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX:
    case TRI_IDX_TYPE_BITARRAY_INDEX:
    case TRI_IDX_TYPE_UNKNOWN: {
    }
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an index id
////////////////////////////////////////////////////////////////////////////////

bool Index::validateId (char const* key) {
  char const* p = key;

  while (1) {
    const char c = *p;

    if (c == '\0') {
      return (p - key) > 0;
    }
    if (c >= '0' && c <= '9') {
      ++p;
      continue;
    }

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an index handle (collection name + / + index id)
////////////////////////////////////////////////////////////////////////////////

bool Index::validateHandle (char const* key,
                            size_t* split) {
  char const* p = key;
  char c = *p;

  // extract collection name

  if (! (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
    return false;
  }

  ++p;

  while (1) {
    c = *p;

    if ((c == '_') || (c == '-') || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      ++p;
      continue;
    }

    if (c == '/') {
      break;
    }

    return false;
  }

  if (p - key > TRI_COL_NAME_LENGTH) {
    return false;
  }

  // store split position
  *split = p - key;
  ++p;

  // validate index id
  return validateId(p);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new index id
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t Index::generateId () {
  return TRI_NewTickServer();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index comparator, used by the coordinator to detect if two index
/// contents are the same
////////////////////////////////////////////////////////////////////////////////

bool Index::Compare (TRI_json_t const* lhs,
                     TRI_json_t const* rhs) {
  TRI_json_t* typeJson = TRI_LookupObjectJson(lhs, "type");
  TRI_ASSERT(TRI_IsStringJson(typeJson));

  // type must be identical
  if (! TRI_CheckSameValueJson(typeJson, TRI_LookupObjectJson(rhs, "type"))) {
    return false;
  }

  auto type = Index::type(typeJson->_value._string.data);

  // unique must be identical if present
  TRI_json_t* value = TRI_LookupObjectJson(lhs, "unique");
  if (TRI_IsBooleanJson(value)) {
    if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "unique"))) {
      return false;
    }
  }

  // sparse must be identical if present
  value = TRI_LookupObjectJson(lhs, "sparse");
  if (TRI_IsBooleanJson(value)) {
    if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "sparse"))) {
      return false;
    }
  }


  if (type == IndexType::TRI_IDX_TYPE_GEO1_INDEX) {
    // geoJson must be identical if present
    value = TRI_LookupObjectJson(lhs, "geoJson");
    if (TRI_IsBooleanJson(value)) {
      if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "geoJson"))) {
        return false;
      }
    }
  }
  else if (type == IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    // minLength
    value = TRI_LookupObjectJson(lhs, "minLength");
    if (TRI_IsNumberJson(value)) {
      if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "minLength"))) {
        return false;
      }
    }
  }
  else if (type == IndexType::TRI_IDX_TYPE_CAP_CONSTRAINT) {
    // size, byteSize
    value = TRI_LookupObjectJson(lhs, "size");
    if (TRI_IsNumberJson(value)) {
      if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "size"))) {
        return false;
      }
    }

    value = TRI_LookupObjectJson(lhs, "byteSize");
    if (TRI_IsNumberJson(value)) {
      if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "byteSize"))) {
        return false;
      }
    }
  }

  // other index types: fields must be identical if present
  value = TRI_LookupObjectJson(lhs, "fields");

  if (TRI_IsArrayJson(value)) {
    if (type == IndexType::TRI_IDX_TYPE_HASH_INDEX) {
      size_t const nv = TRI_LengthArrayJson(value);
 
      // compare fields in arbitrary order
      TRI_json_t const* r = TRI_LookupObjectJson(rhs, "fields");

      if (! TRI_IsArrayJson(r) ||
          nv != TRI_LengthArrayJson(r)) {
        return false;
      }

      size_t const nr = TRI_LengthArrayJson(r);

      for (size_t i = 0; i < nv; ++i) {
        TRI_json_t const* v = TRI_LookupArrayJson(value, i);

        bool found = false;

        for (size_t j = 0; j < nr; ++j) {
          if (TRI_CheckSameValueJson(v, TRI_LookupArrayJson(r, j))) {
            found = true;
            break;
          }
        }

        if (! found) {
          return false;
        }
      }
    }
    else {
      if (! TRI_CheckSameValueJson(value, TRI_LookupObjectJson(rhs, "fields"))) {
        return false;
      }
    }

  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a contextual string for logging
////////////////////////////////////////////////////////////////////////////////

std::string Index::context () const {
  std::ostringstream result;

  result << "index { id: " << id() 
         << ", type: " << typeName() 
         << ", collection: " << _collection->_vocbase->_name 
         << "/" <<  _collection->_info._name << " }";

  return result.str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON representation of the index
/// base functionality (called from derived classes)
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json Index::toJson (TRI_memory_zone_t* zone) const {
  triagens::basics::Json json(zone, triagens::basics::Json::Object, 4);

  json("id", triagens::basics::Json(zone, std::to_string(_iid)))
      ("type", triagens::basics::Json(zone, typeName()));

  if (dumpFields()) {
    triagens::basics::Json f(zone, triagens::basics::Json::Array, fields().size());

    for (auto const& field : fields()) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      f.add(triagens::basics::Json(zone, fieldString));
    }

    json("fields", f);
  }

  if (hasSelectivityEstimate()) {
    json("selectivityEstimate", triagens::basics::Json(selectivityEstimate()));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for selectivityEstimate
////////////////////////////////////////////////////////////////////////////////

double Index::selectivityEstimate () const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for selectivityEstimate
////////////////////////////////////////////////////////////////////////////////

int Index::batchInsert (std::vector<struct TRI_doc_mptr_t const*> const*, size_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for postInsert
////////////////////////////////////////////////////////////////////////////////

int Index::postInsert (struct TRI_transaction_collection_s*,
                       struct TRI_doc_mptr_t const*) {
  // do nothing
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for cleanup
////////////////////////////////////////////////////////////////////////////////

int Index::cleanup () {
  // do nothing
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for sizeHint
////////////////////////////////////////////////////////////////////////////////

int Index::sizeHint (size_t) {
  // do nothing
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for hasBatchInsert
////////////////////////////////////////////////////////////////////////////////

bool Index::hasBatchInsert () const {
  return false;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to insert a document into any index type
////////////////////////////////////////////////////////////////////////////////

int Index::fillElement(std::function<TRI_index_element_t* ()> allocate,
                       std::vector<TRI_index_element_t*>& elements,
                       TRI_doc_mptr_t const* document,
                       std::vector<TRI_shape_pid_t> const& paths,
                       bool const sparse) {
  TRI_ASSERT(document != nullptr);
  TRI_ASSERT_EXPENSIVE(document->getDataPtr() != nullptr);   // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shaped_json_t shapedJson;

  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (shapedJson._sid == TRI_SHAPE_ILLEGAL) {
    LOG_WARNING("encountered invalid marker with shape id 0");

    return TRI_ERROR_INTERNAL;
  }

  char const* ptr = document->getShapedJsonPtr();  // ONLY IN INDEX, PROTECTED by RUNTIME

  size_t const n = paths.size();

  TRI_index_element_t* element = allocate();
  if (element == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  element->document(const_cast<TRI_doc_mptr_t*>(document));
  TRI_shaped_sub_t* subObjects = element->subObjects();
  for (size_t j = 0; j < n; ++j) {
    TRI_shape_pid_t path = paths[j];

    // ..........................................................................
    // Determine if document has that particular shape
    // ..........................................................................

    TRI_shape_access_t const* acc = _collection->getShaper()->findAccessor(shapedJson._sid, path);  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
      // OK, the document does not contain the attributed needed by 
      // the index, are we sparse?
      subObjects[j]._sid = BasicShapes::TRI_SHAPE_SID_NULL;


      if (sparse) {
        // no need to continue
        // Free this element and return.
        TRI_index_element_t::free(element);
        return TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;
      }
      continue;
    }

    // ..........................................................................
    // Extract the field
    // ..........................................................................

    TRI_shaped_json_t shapedObject;
    if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
      return TRI_ERROR_INTERNAL;
    }

    if (shapedObject._sid == BasicShapes::TRI_SHAPE_SID_NULL) {
      if (sparse) {
        // no need to continue
        // Free this element and return.
        TRI_index_element_t::free(element);
        return TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;
      }
    }

    // .........................................................................
    // Store the field
    // .........................................................................

    TRI_FillShapedSub(&subObjects[j], &shapedObject, ptr);
  }
  elements.push_back(element);

  return TRI_ERROR_NO_ERROR;
}

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index description to an output stream
////////////////////////////////////////////////////////////////////////////////
     
    std::ostream& operator<< (std::ostream& stream,
                              Index const* index) {
      stream << index->context();
      return stream;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index description to an output stream
////////////////////////////////////////////////////////////////////////////////

    std::ostream& operator<< (std::ostream& stream,
                              Index const& index) {
      stream << index.context();
      return stream;
    }

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
