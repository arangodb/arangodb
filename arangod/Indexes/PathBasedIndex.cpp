////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "PathBasedIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Logger.h"

using namespace arangodb;

arangodb::aql::AstNode const* PathBasedIndex::PermutationState::getValue()
    const {
  if (type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
    TRI_ASSERT(current == 0);
    return value;
  } else if (type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
    TRI_ASSERT(n > 0);
    TRI_ASSERT(current < n);
    return value->getMember(current);
  }

  TRI_ASSERT(false);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

PathBasedIndex::PathBasedIndex(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse, bool allowPartialIndex)
    : Index(iid, collection, fields, unique, sparse),
      _shaper(_collection->getShaper()),
      _paths(fillPidPaths()),
      _useExpansion(false),
      _allowPartialIndex(allowPartialIndex) {
  TRI_ASSERT(!fields.empty());

  TRI_ASSERT(iid != 0);

  for (auto const& it : fields) {
    if (TRI_AttributeNamesHaveExpansion(it)) {
      _useExpansion = true;
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

PathBasedIndex::PathBasedIndex(VPackSlice const& slice, bool allowPartialIndex)
    : Index(slice),
      _shaper(nullptr),
      _paths(),
      _useExpansion(false),
      _allowPartialIndex(allowPartialIndex) {
  TRI_ASSERT(!_fields.empty());

  for (auto const& it : _fields) {
    if (TRI_AttributeNamesHaveExpansion(it)) {
      _useExpansion = true;
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the index
////////////////////////////////////////////////////////////////////////////////

PathBasedIndex::~PathBasedIndex() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to insert a document into any index type
////////////////////////////////////////////////////////////////////////////////

int PathBasedIndex::fillElement(std::vector<TRI_index_element_t*>& elements,
                                TRI_doc_mptr_t const* document) {
  TRI_ASSERT(document != nullptr);
  TRI_ASSERT(document->getDataPtr() !=
                       nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shaped_json_t shapedJson;

  TRI_EXTRACT_SHAPED_JSON_MARKER(
      shapedJson,
      document->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (shapedJson._sid == TRI_SHAPE_ILLEGAL) {
    LOG(WARN) << "encountered invalid marker with shape id 0";

    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalShape") { return TRI_ERROR_INTERNAL; }

  size_t const n = _paths.size();
  std::vector<TRI_shaped_json_t> shapes;

  if (!_useExpansion) {
    // fast path for inserts... no array elements used
    buildIndexValue(&shapedJson, shapes);

    if (shapes.size() == n) {
      // if shapes.size() != n, then the value is not inserted into the index
      // because of
      // index sparsity!
      char const* ptr =
          document->getShapedJsonPtr();  // ONLY IN INDEX, PROTECTED by RUNTIME

      TRI_index_element_t* element = TRI_index_element_t::allocate(n);

      if (element == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
      TRI_IF_FAILURE("FillElementOOM") {
        // clean up manually
        TRI_index_element_t::freeElement(element);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      element->document(const_cast<TRI_doc_mptr_t*>(document));
      TRI_shaped_sub_t* subObjects = element->subObjects();

      for (size_t i = 0; i < n; ++i) {
        TRI_FillShapedSub(&subObjects[i], &shapes[i], ptr);
      }

      try {
        TRI_IF_FAILURE("FillElementOOM2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }

        elements.emplace_back(element);
      } catch (...) {
        TRI_index_element_t::freeElement(element);
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    // other path for handling array elements, too
    std::unordered_set<std::vector<TRI_shaped_json_t>> toInsert;

    buildIndexValues(&shapedJson, &shapedJson, 0, 0, toInsert, shapes, false);

    if (!toInsert.empty()) {
      char const* ptr =
          document->getShapedJsonPtr();  // ONLY IN INDEX, PROTECTED by RUNTIME

      elements.reserve(toInsert.size());

      for (auto& info : toInsert) {
        TRI_ASSERT(info.size() == n);
        TRI_index_element_t* element = TRI_index_element_t::allocate(n);

        if (element == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }
        TRI_IF_FAILURE("FillElementOOM") {
          // clean up manually
          TRI_index_element_t::freeElement(element);
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        element->document(const_cast<TRI_doc_mptr_t*>(document));
        TRI_shaped_sub_t* subObjects = element->subObjects();

        for (size_t j = 0; j < n; ++j) {
          TRI_FillShapedSub(&subObjects[j], &info[j], ptr);
        }

        try {
          TRI_IF_FAILURE("FillElementOOM2") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          elements.emplace_back(element);
        } catch (...) {
          TRI_index_element_t::freeElement(element);
          return TRI_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to create the sole index value insert
////////////////////////////////////////////////////////////////////////////////

void PathBasedIndex::buildIndexValue(TRI_shaped_json_t const* documentShape,
                                     std::vector<TRI_shaped_json_t>& shapes) {
  TRI_shape_t const* shape = nullptr;

  size_t const n = _paths.size();

  for (size_t i = 0; i < n; ++i) {
    TRI_ASSERT(!_paths[i].empty());

    TRI_shaped_json_t shapedJson;
    TRI_shape_pid_t pid = _paths[i][0].first;

    bool check =
        _shaper->extractShapedJson(documentShape, 0, pid, &shapedJson, &shape);

    if (!check || shape == nullptr ||
        shapedJson._sid == BasicShapes::TRI_SHAPE_SID_NULL) {
      // attribute not, found
      if (_sparse) {
        // If sparse we do not have to index
        return;
      }

      // If not sparse we insert null here
      shapedJson._sid = BasicShapes::TRI_SHAPE_SID_NULL;
      shapedJson._data.data = nullptr;
      shapedJson._data.length = 0;
    }

    shapes.emplace_back(shapedJson);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to create a set of index combinations to insert
////////////////////////////////////////////////////////////////////////////////

void PathBasedIndex::buildIndexValues(
    TRI_shaped_json_t const* documentShape, TRI_shaped_json_t const* subShape,
    size_t subShapeLevel, size_t level,
    std::unordered_set<std::vector<TRI_shaped_json_t>>& toInsert,
    std::vector<TRI_shaped_json_t>& shapes, bool insertIllegals) {
  TRI_ASSERT(level < _paths.size());
  TRI_shaped_json_t currentShape = *subShape;
  TRI_shape_t const* shape = nullptr;
  if (insertIllegals) {
    TRI_ASSERT(_allowPartialIndex);
    currentShape._sid = BasicShapes::TRI_SHAPE_SID_ILLEGAL;
    currentShape._data.data = nullptr;
    currentShape._data.length = 0;
  } else {
    size_t const n = _paths[level].size();
    size_t i = subShapeLevel;

    while (i < n) {
      TRI_shaped_json_t shapedJson;
      TRI_shape_pid_t pid = _paths[level][i].first;

      bool check =
          _shaper->extractShapedJson(subShape, 0, pid, &shapedJson, &shape);

      bool expand = _paths[level][i].second;

      if (!check || shape == nullptr ||
          shapedJson._sid == BasicShapes::TRI_SHAPE_SID_NULL) {
        // attribute not, found
        bool expandAnywhere = false;
        size_t k = 0;
        for (; k < n; ++k) {
          if (_paths[level][k].second) {
            expandAnywhere = true;
            break;
          }
        }
        if (expandAnywhere && i <= k) {
          // We have an array index and we are not evaluating the indexed
          // attribute.
          // Do not index this attribute at all
          if (level == 0) {
            // If we expand at the first position and this is not set we cannot
            // index anything
            return;
          }
          if (!_allowPartialIndex) {
            // We do not allow partial Indexing. index nothing here
            return;
          }
          // We allow partial indexing. First elements are ok,
          // last will be stored as illegal
          // We insert ILLEGAL_SHAPES for all the following elements
          insertIllegals = true;
          currentShape._sid = BasicShapes::TRI_SHAPE_SID_ILLEGAL;
          currentShape._data.data = nullptr;
          currentShape._data.length = 0;
          break;
        }
        if (!expandAnywhere && _sparse) {
          // If sparse we do not have to index.
          // If we are in an array we index null
          return;
        }

        // If not sparse we insert null here
        currentShape._sid = BasicShapes::TRI_SHAPE_SID_NULL;
        currentShape._data.data = nullptr;
        currentShape._data.length = 0;
        break;
      }

      if (expand) {
        size_t len = 0;
        TRI_shaped_json_t shapedArrayElement;
        bool ok = false;
        switch (shape->_type) {
          case TRI_SHAPE_LIST:
            len = TRI_LengthListShapedJson((const TRI_list_shape_t*)shape,
                                           &shapedJson);
            if (len == 0) {
              break;
            }
            for (size_t index = 0; index < len; ++index) {
              ok =
                  TRI_AtListShapedJson((const TRI_list_shape_t*)shape,
                                       &shapedJson, index, &shapedArrayElement);
              if (ok) {
                buildIndexValues(documentShape, &shapedArrayElement, i + 1,
                                 level, toInsert, shapes, false);
              } else {
                TRI_ASSERT(false);
              }
            }
            // Leave the while loop here, it has been walked through in
            // recursion
            return;
          case TRI_SHAPE_HOMOGENEOUS_LIST:
            len = TRI_LengthHomogeneousListShapedJson(
                (const TRI_homogeneous_list_shape_t*)shape, &shapedJson);
            if (len == 0) {
              break;
            }
            for (size_t index = 0; index < len; ++index) {
              ok = TRI_AtHomogeneousListShapedJson(
                  (const TRI_homogeneous_list_shape_t*)shape, &shapedJson,
                  index, &shapedArrayElement);
              if (ok) {
                buildIndexValues(documentShape, &shapedArrayElement, i + 1,
                                 level, toInsert, shapes, false);
              } else {
                TRI_ASSERT(false);
              }
            }
            // Leave the while loop here, it has been walked through in
            // recursion
            return;
          case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
            len = TRI_LengthHomogeneousSizedListShapedJson(
                (const TRI_homogeneous_sized_list_shape_t*)shape, &shapedJson);
            if (len == 0) {
              break;
            }
            for (size_t index = 0; index < len; ++index) {
              ok = TRI_AtHomogeneousSizedListShapedJson(
                  (const TRI_homogeneous_sized_list_shape_t*)shape, &shapedJson,
                  index, &shapedArrayElement);
              if (ok) {
                buildIndexValues(documentShape, &shapedArrayElement, i + 1,
                                 level, toInsert, shapes, false);
              } else {
                TRI_ASSERT(false);
              }
            }
            // Leave the while loop here, it has been walked through in
            // recursion
            return;
        }
        // Non Array attribute or empty array cannot be expanded
        if (_allowPartialIndex && level > 0) {
          // If we have a partial index we can index everything up to this level
          insertIllegals = true;
          currentShape._sid = BasicShapes::TRI_SHAPE_SID_ILLEGAL;
          currentShape._data.data = nullptr;
          currentShape._data.length = 0;
          // We have to leave the loop here.
          break;
        }
        // We cannot index anything. Return here
        return;
      } else {
        currentShape = shapedJson;
      }
      ++i;
    }
  }

  shapes.emplace_back(currentShape);

  if (level + 1 == _paths.size()) {
    toInsert.emplace(shapes);
    shapes.pop_back();
    return;
  }

  buildIndexValues(documentShape, documentShape, 0, level + 1, toInsert, shapes,
                   insertIllegals);
  shapes.pop_back();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to transform AttributeNames into pid lists
/// This will create PIDs for all indexed Attributes
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>>
PathBasedIndex::fillPidPaths() {
  TRI_ASSERT(_shaper != nullptr);

  std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> result;

  for (auto const& list : _fields) {
    std::vector<std::pair<TRI_shape_pid_t, bool>> interior;
    std::vector<std::string> joinedNames;
    TRI_AttributeNamesJoinNested(list, joinedNames, false);

    for (size_t i = 0; i < joinedNames.size(); ++i) {
      auto pid =
          _shaper->findOrCreateAttributePathByName(joinedNames[i].c_str());
      interior.emplace_back(pid, i < joinedNames.size() - 1);
    }
    result.emplace_back(interior);
  }

  return result;
}
