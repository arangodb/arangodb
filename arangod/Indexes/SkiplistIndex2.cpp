////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
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

#include "SkiplistIndex2.h"
#include "Basics/logging.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/VocShaper.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

static int FillLookupOperator (TRI_index_operator_t* slOperator,
                               TRI_document_collection_t* document) {
  if (slOperator == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  switch (slOperator->_type) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* logicalOperator = (TRI_logical_index_operator_t*) slOperator;
      int res = FillLookupOperator(logicalOperator->_left, document);

      if (res == TRI_ERROR_NO_ERROR) {
        res = FillLookupOperator(logicalOperator->_right, document);
      }
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* relationOperator = (TRI_relation_index_operator_t*) slOperator;
      relationOperator->_numFields = TRI_LengthVector(&relationOperator->_parameters->_value._objects);
      relationOperator->_fields = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * relationOperator->_numFields, false));

      if (relationOperator->_fields != nullptr) {
        for (size_t j = 0; j < relationOperator->_numFields; ++j) {
          TRI_json_t const* jsonObject = static_cast<TRI_json_t* const>(TRI_AtVector(&(relationOperator->_parameters->_value._objects), j));

          // find out if the search value is a list or an array
          if ((TRI_IsArrayJson(jsonObject) || TRI_IsObjectJson(jsonObject)) &&
              slOperator->_type != TRI_EQ_INDEX_OPERATOR) {
            // non-equality operator used on list or array data type, this is disallowed
            // because we need to shape these objects first. however, at this place (index lookup)
            // we never want to create new shapes so we will have a problem if we cannot find an
            // existing shape for the search value. in this case we would need to raise an error
            // but then the query results would depend on the state of the shaper and if it had
            // seen previous such objects

            // we still allow looking for list or array values using equality. this is safe.
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields);
            relationOperator->_fields = nullptr;
            return TRI_ERROR_BAD_PARAMETER;
          }

          // now shape the search object (but never create any new shapes)
          TRI_shaped_json_t* shapedObject = TRI_ShapedJsonJson(document->getShaper(), jsonObject, false);  // ONLY IN INDEX, PROTECTED by RUNTIME

          if (shapedObject != nullptr) {
            // found existing shape
            relationOperator->_fields[j] = *shapedObject; // shallow copy here is ok
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, shapedObject); // don't require storage anymore
          }
          else {
            // shape not found
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields);
            relationOperator->_fields = nullptr;
            return TRI_RESULT_ELEMENT_NOT_FOUND;
          }
        }
      }
      else {
        relationOperator->_numFields = 0; // out of memory?
      }
      break;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               class SkiplistIndex
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

SkiplistIndex2::SkiplistIndex2 (TRI_idx_iid_t iid,
                                TRI_document_collection_t* collection,
                                std::vector<std::string> const& fields,
                                std::vector<TRI_shape_pid_t> const& paths,
                                bool unique,
                                bool sparse) 
  : Index(iid, collection, fields),
    _paths(paths),
    _skiplistIndex(nullptr),
    _unique(unique),
    _sparse(sparse) {
  
  TRI_ASSERT(iid != 0);
  
  _skiplistIndex = SkiplistIndex_new(collection,
                                     paths.size(),
                                     unique);
}

SkiplistIndex2::~SkiplistIndex2 () {
  if (_skiplistIndex != nullptr) {
    SkiplistIndex_free(_skiplistIndex);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
        
size_t SkiplistIndex2::memory () const {
  return SkiplistIndex_memoryUsage(_skiplistIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////
        
triagens::basics::Json SkiplistIndex2::toJson (TRI_memory_zone_t* zone,
                                               bool withFigures) const {
  auto json = Index::toJson(zone, withFigures);

  json("unique", triagens::basics::Json(zone, _unique))
      ("sparse", triagens::basics::Json(zone, _sparse));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index figures
////////////////////////////////////////////////////////////////////////////////
        
triagens::basics::Json SkiplistIndex2::toJsonFigures (TRI_memory_zone_t* zone) const {
  triagens::basics::Json json(triagens::basics::Json::Object);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skiplist index
////////////////////////////////////////////////////////////////////////////////
  
int SkiplistIndex2::insert (TRI_doc_mptr_t const* doc, 
                            bool) {
  auto skiplistElement = static_cast<TRI_skiplist_index_element_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, SkiplistIndex_ElementSize(_skiplistIndex), false));

  if (skiplistElement == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = fillElement(skiplistElement, doc);
  // ...........................................................................
  // most likely the cause of this error is that the index is sparse
  // and not all attributes the index needs are set -- so the document
  // is ignored. So not really an error at all. Note that this does
  // not happen in a non-sparse skiplist index, in which empty
  // attributes are always treated as if they were bound to null, so
  // TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING cannot happen at
  // all.
  // ...........................................................................

  // .........................................................................
  // It may happen that the document does not have the necessary
  // attributes to be included within the hash index, in this case do
  // not report back an error.
  // .........................................................................

  if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
    if (_sparse) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement);
      return TRI_ERROR_NO_ERROR;
    }

    res = TRI_ERROR_NO_ERROR;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement);
    return res;
  }

  // insert into the index. the memory for the element will be owned or freed
  // by the index
  return SkiplistIndex_insert(_skiplistIndex, skiplistElement);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////
         
int SkiplistIndex2::remove (TRI_doc_mptr_t const* doc, 
                            bool) {
  auto skiplistElement = static_cast<TRI_skiplist_index_element_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, SkiplistIndex_ElementSize(_skiplistIndex), false));

  if (skiplistElement == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = fillElement(skiplistElement, doc);

  // ..........................................................................
  // Error returned generally implies that the document never was part of the
  // skiplist index
  // ..........................................................................
  if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
    if (_sparse) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement);
      return TRI_ERROR_NO_ERROR;
    }

    res = TRI_ERROR_NO_ERROR;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement);
    return res;
  }

  // attempt the removal for skiplist indexes
  // ownership for the index element is transferred to the index

  return SkiplistIndex_remove(_skiplistIndex, skiplistElement);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
///
/// Note: this function will not destroy the passed slOperator before it returns
/// Warning: who ever calls this function is responsible for destroying
/// the TRI_index_operator_t* and the TRI_skiplist_iterator_t* results
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_iterator_t* SkiplistIndex2::lookup (TRI_index_operator_t* slOperator,
                                                 bool reverse) {
  if (slOperator == nullptr) {
    return nullptr;
  }

  // .........................................................................
  // fill the relation operators which may be embedded in the slOperator with
  // additional information. Recall the slOperator is what information was
  // received from a user for query the skiplist.
  // .........................................................................

  int res = FillLookupOperator(slOperator, _collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  return SkiplistIndex_find(_skiplistIndex,
                            slOperator, 
                            reverse);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for skiplist methods
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex2::fillElement(TRI_skiplist_index_element_t* skiplistElement,
                                TRI_doc_mptr_t const* document) {
  // ..........................................................................
  // Assign the document to the SkiplistIndexElement structure so that it can
  // be retrieved later.
  // ..........................................................................

  TRI_ASSERT(document != nullptr);
  TRI_ASSERT_EXPENSIVE(document->getDataPtr() != nullptr);   // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (shapedJson._sid == TRI_SHAPE_ILLEGAL) {
    LOG_WARNING("encountered invalid marker with shape id 0");

    return TRI_ERROR_INTERNAL;
  }

  int res = TRI_ERROR_NO_ERROR;

  skiplistElement->_document = const_cast<TRI_doc_mptr_t*>(document);
  char const* ptr = skiplistElement->_document->getShapedJsonPtr();  // ONLY IN INDEX, PROTECTED by RUNTIME

  auto subObjects = SkiplistIndex_Subobjects(skiplistElement);

  size_t const n = _paths.size();

  for (size_t j = 0; j < n; ++j) {
    TRI_shape_pid_t shape = _paths[j];

    // ..........................................................................
    // Determine if document has that particular shape
    // ..........................................................................

    TRI_shape_access_t const* acc = _collection->getShaper()->findAccessor(shapedJson._sid, shape);  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
      // OK, the document does not contain the attributed needed by 
      // the index, are we sparse?
      subObjects[j]._sid = BasicShapes::TRI_SHAPE_SID_NULL; 

      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;

      if (_sparse) {
        // no need to continue
        return res;
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
      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;

      if (_sparse) {
        // no need to continue
        return res;
      }
    }

    // .........................................................................
    // Store the field
    // .........................................................................

    TRI_FillShapedSub(&subObjects[j], &shapedObject, ptr);
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
