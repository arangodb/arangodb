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

////////////////////////////////////////////////////////////////////////////////
/// @brief create the skiplist index
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex2::SkiplistIndex2 (TRI_idx_iid_t iid,
                                TRI_document_collection_t* collection,
                                std::vector<std::vector<triagens::basics::AttributeName>> const& fields,
                                bool unique,
                                bool sparse) 
  : PathBasedIndex(iid, collection, fields, unique, sparse),
    _skiplistIndex(nullptr) {

  _skiplistIndex = SkiplistIndex_new(collection,
                                     _paths.size(),
                                     unique,
                                     _useExpansion);

  if (_skiplistIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the skiplist index
////////////////////////////////////////////////////////////////////////////////

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
  json("memory", triagens::basics::Json(static_cast<double>(memory())));
  _skiplistIndex->skiplist->appendToJson(zone, json);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skiplist index
////////////////////////////////////////////////////////////////////////////////
  
int SkiplistIndex2::insert (TRI_doc_mptr_t const* doc, 
                            bool) {

  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(SkiplistIndex_ElementSize(_skiplistIndex), false);
  };
  std::vector<TRI_index_element_t*> elements;

  int res = fillElement(allocate, elements, doc);

  // insert into the index. the memory for the element will be owned or freed
  // by the index

  size_t count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    res = _skiplistIndex->skiplist->insert(elements[i]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_index_element_t::free(elements[i]);
      // Note: this element is freed already
      for (size_t j = i + 1; j < count; ++j) {
        TRI_index_element_t::free(elements[j]);
      }
      for (size_t j = 0; j < i; ++j) {
         _skiplistIndex->skiplist->remove(elements[j]);
        // No need to free elements[j] skiplist has taken over already
      }
      return res;
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////
         
int SkiplistIndex2::remove (TRI_doc_mptr_t const* doc, 
                            bool) {

  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(SkiplistIndex_ElementSize(_skiplistIndex), false);
  };

  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(allocate, elements, doc);

  // attempt the removal for skiplist indexes
  // ownership for the index element is transferred to the index

  size_t count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    res = _skiplistIndex->skiplist->remove(elements[i]);
    TRI_index_element_t::free(elements[i]);
  }
  return res;
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
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
