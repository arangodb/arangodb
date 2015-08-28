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





////////////////////////////////////////////////////////////////////////////////
/// @brief frees an element in the skiplist
////////////////////////////////////////////////////////////////////////////////

static void FreeElm (void* e) {
  auto element = static_cast<TRI_index_element_t*>(e);
  TRI_index_element_t::free(element);
}


// .............................................................................
// recall for all of the following comparison functions:
//
// left < right  return -1
// left > right  return  1
// left == right return  0
//
// furthermore:
//
// the following order is currently defined for placing an order on documents
// undef < null < boolean < number < strings < lists < hash arrays
// note: undefined will be treated as NULL pointer not NULL JSON OBJECT
// within each type class we have the following order
// boolean: false < true
// number: natural order
// strings: lexicographical
// lists: lexicographically and within each slot according to these rules.
// ...........................................................................

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key with an element, version with proper types
////////////////////////////////////////////////////////////////////////////////

static int CompareKeyElement (TRI_shaped_json_t const* left,
                              TRI_index_element_t const* right,
                              size_t rightPosition,
                              VocShaper* shaper) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  auto rightSubobjects = right->subObjects();

  return TRI_CompareShapeTypes(nullptr,
                               nullptr,
                               left,
                               shaper,
                               right->document()->getShapedJsonPtr(),
                               &rightSubobjects[rightPosition],
                               nullptr,
                               shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key with an element in a skip list, generic callback
////////////////////////////////////////////////////////////////////////////////

static int CmpKeyElm (void* sli,
                      TRI_skiplist_index_key_t const* leftKey,
                      TRI_index_element_t const* rightElement) {

  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);

  triagens::arango::SkiplistIndex2* skiplistindex = static_cast<triagens::arango::SkiplistIndex2*>(sli);
  auto shaper = skiplistindex->collection()->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  // Note that the key might contain fewer fields than there are indexed
  // attributes, therefore we only run the following loop to
  // leftKey->_numFields.
  for (size_t j = 0;  j < leftKey->_numFields;  j++) {
    int compareResult = CompareKeyElement(&leftKey->_fields[j], rightElement, j, shaper);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief compares elements, version with proper types
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement (TRI_index_element_t const* left,
                                  size_t leftPosition,
                                  TRI_index_element_t const* right,
                                  size_t rightPosition,
                                  VocShaper* shaper) {
  TRI_ASSERT(nullptr != left);
  TRI_ASSERT(nullptr != right);
  
  auto leftSubobjects = left->subObjects();
  auto rightSubobjects = right->subObjects();

  return TRI_CompareShapeTypes(left->document()->getShapedJsonPtr(),
                               &leftSubobjects[leftPosition],
                               nullptr,
                               shaper,
                               right->document()->getShapedJsonPtr(),
                               &rightSubobjects[rightPosition],
                               nullptr,
                               shaper);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list, this is the generic callback
////////////////////////////////////////////////////////////////////////////////

static int CmpElmElm (void* sli,
                      TRI_index_element_t const* leftElement,
                      TRI_index_element_t const* rightElement,
                      triagens::basics::SkipListCmpType cmptype) {

  TRI_ASSERT(nullptr != leftElement);
  TRI_ASSERT(nullptr != rightElement);

  // ..........................................................................
  // The document could be the same -- so no further comparison is required.
  // ..........................................................................

  SkiplistIndex* skiplistindex = static_cast<SkiplistIndex*>(sli);

  if (leftElement == rightElement ||
      (! skiplistindex->skiplist->isArray() && leftElement->document() == rightElement->document())) {
    return 0;
  }

  auto shaper = skiplistindex->_collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  for (size_t j = 0;  j < skiplistindex->_numFields;  j++) {
    int compareResult = CompareElementElement(leftElement,
                                              j,
                                              rightElement,
                                              j,
                                              shaper);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  // ...........................................................................
  // This is where the difference between the preorder and the proper total
  // order comes into play. Here if the 'keys' are the same,
  // but the doc ptr is different (which it is since we are here), then
  // we return 0 if we use the preorder and look at the _key attribute
  // otherwise.
  // ...........................................................................

  if (triagens::basics::SKIPLIST_CMP_PREORDER == cmptype) {
    return 0;
  }

  // We break this tie in the key comparison by looking at the key:
  int compareResult = strcmp(TRI_EXTRACT_MARKER_KEY(leftElement->document()),    // ONLY IN INDEX, PROTECTED by RUNTIME
                             TRI_EXTRACT_MARKER_KEY(rightElement->document()));  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (compareResult < 0) {
    return -1;
  }
  else if (compareResult > 0) {
    return 1;
  }
  return 0;
}

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
// --SECTION--                                            class SkiplistIterator
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

size_t SkiplistIterator::size () {
  return _intervals.size();
}

void SkiplistIterator::initCursor () {
  size_t const n =  _intervals.size();
  if (0 < n) {
    if (_reverse) {
      // start at last interval, right endpoint
      _currentInterval = n - 1;
      _cursor = _intervals[n -1]->_rightEndPoint;
    }
    else {
      // start at first interval, left endpoint
      _currentInterval = 0;
      _cursor = _intervals[0]->_leftEndPoint;
    }
  }
  else {
    cursor = nullptr;
  }
}

bool SkiplistIterator::hasNext () {
  if (_reverse) {
    return hasPrevIteration();
  }
  return hasNextIteration();
}

bool SkiplistIterator::next () {
  if (_reverse) {
    return prevIteration();
  }
  return nextIteration();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a previous document within an
/// interval or before it - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

bool SkiplistIterator::hasPrevIteration () {
  // ...........................................................................
  // if we have more intervals than the one we are currently working
  // on then of course we have a previous doc, because intervals are nonempty.
  // ...........................................................................
  if (_currentInterval > 0) {
    return true;
  }

  Node const* leftNode = _index->prevNode(_cursor);

  // Note that leftNode can be nullptr here!
  // ...........................................................................
  // If the leftNode == left end point AND there are no more intervals
  // then we have no next.
  // ...........................................................................
  return leftNode != _intervals[_currentInterval]->_leftEndPoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a next document within an
/// interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

bool SkiplistIterator::hasNextIteration () {
  if (_cursor == nullptr) {
    return false;
  }

  // ...........................................................................
  // if we have more intervals than the one we are currently working
  // on then of course we have a next doc, since intervals are nonempty.
  // ...........................................................................
  if (_intervals.size() - 1 > _currentInterval) {
    return true;
  }

  Node const* leftNode = _cursor->nextNode();

  // Note that leftNode can be nullptr here!
  // ...........................................................................
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ...........................................................................
  return leftNode != _intervals[_currentInterval]->_rightEndPoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps backwards by jumpSize and returns the document
////////////////////////////////////////////////////////////////////////////////

TRI_index_element_t* SkiplistIterator::prevIteration () {
  static const int64_t jumpSize = 1;

  TRI_skiplist_iterator_interval_t* interval = _intervals[_currentInterval];

  if (interval == nullptr) {
    return nullptr;
  }

  // ...........................................................................
  // use the current cursor and move jumpSize backward
  // ...........................................................................

  Node* result = nullptr; 

  result = _index->prevNode(_cursor);

  if (result == interval->_leftEndPoint) {
    if (_currentInterval == 0) {
      _cursor = nullptr;  // exhausted
      return nullptr;
    }
    --_currentInterval;
    interval = _intervals[_currentInterval];
    TRI_ASSERT(interval != nullptr);
    _cursor = _rightEndPoint;
    result = _index->prevNode(_cursor);
  }
  _cursor = result;

  TRI_ASSERT(result != nullptr);
  return result->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps forwards by jumpSize and returns the document
////////////////////////////////////////////////////////////////////////////////

TRI_index_element_t* SkiplistIterator::nextIteration () {

  if (_cursor == nullptr) {
    // In this case the iterator is exhausted or does not even have intervals.
    return nullptr;
  }
  
  TRI_skiplist_iterator_interval_t* interval = _intervals[_currentInterval];

  if (interval == nullptr) {
    return nullptr;
  }

  while (true) {   // will be left by break
    _cursor = _cursor->nextNode();
    if (_cursor != _rightEndPoint) {
      // Note that _cursor can be nullptr here!
      break;   // we found a next one
    }
    if (_currentInterval == _intervals.size() - 1) {
      _cursor = nullptr;  // exhausted
      return nullptr;
    }
    ++_currentInterval;
    interval = _intervals[_currentInterval];
    TRI_ASSERT(interval != nullptr);
    _cursor = interval->_leftEndPoint;
  }

  return _cursor->document();
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

  _skiplistIndex = new triagens::basics::SkipList(
                                           CmpElmElm, CmpKeyElm, skiplistIndex,
                                           FreeElm, unique, _useExpansion);
  if (_skiplistIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the skiplist index
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex2::~SkiplistIndex2 () {
  if (_skiplistIndex != nullptr) {
    delete _skiplistIndex;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
        
size_t SkiplistIndex2::memory () const {
  return _skiplistIndex->memoryUsage() +
         static_cast<size_t>(_skiplistIndex->getNrUsed()) * elementSize();
}

size_t SkiplistIndex2::numFields () const {
  return _fields.size();
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

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      TRI_index_element_t::free(it);
    }

    return res;
  }

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

      break;
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

SkiplistIterator* SkiplistIndex2::lookup (TRI_index_operator_t* slOperator,
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
  std::unique_ptr<SkiplistIterator> results = new SkiplistIterator(_skiplistIndex, reverse);

  if (!results) {
    // Check if we could not get an iterator.
    return nullptr; // calling procedure needs to care when the iterator is null
  }

  result->findHelper(slOperator, &(results->_intervals));

  results->initCursor();

  // Finally initialise _cursor if the result is not empty:
  return results;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

size_t SkiplistIndex::elementSize () const {
  return sizeof(TRI_doc_mptr_t*) + (sizeof(TRI_shaped_sub_t) * numFields());
}



// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
