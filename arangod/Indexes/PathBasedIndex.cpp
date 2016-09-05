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
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

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
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse, bool allowPartialIndex)
    : Index(iid, collection, fields, unique, sparse),
      _useExpansion(false),
      _allowPartialIndex(allowPartialIndex) {
  TRI_ASSERT(!fields.empty());

  TRI_ASSERT(iid != 0);

  fillPaths(_paths, _expanding);

  for (auto const& it : fields) {
    if (TRI_AttributeNamesHaveExpansion(it)) {
      _useExpansion = true;
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

PathBasedIndex::PathBasedIndex(TRI_idx_iid_t iid,
                               arangodb::LogicalCollection* collection,
                               VPackSlice const& info, bool allowPartialIndex)
    : Index(iid, collection, info),
      _useExpansion(false),
      _allowPartialIndex(allowPartialIndex) {
  TRI_ASSERT(!_fields.empty());

  TRI_ASSERT(iid != 0);

  fillPaths(_paths, _expanding);

  for (auto const& it : _fields) {
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
  TRI_ASSERT(document->vpack() != nullptr);

  VPackSlice const slice(document->vpack());

  if (slice.isNone()) {
    LOG(WARN) << "encountered invalid marker with slice of type None";

    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalSlice") { return TRI_ERROR_INTERNAL; }

  size_t const n = _paths.size();

  if (!_useExpansion) {
    // fast path for inserts... no array elements used
    auto slices = buildIndexValue(slice);

    if (slices.size() == n) {
      // if shapes.size() != n, then the value is not inserted into the index
      // because of index sparsity!
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
      TRI_vpack_sub_t* subObjects = element->subObjects();

      for (size_t i = 0; i < n; ++i) {
        TRI_FillVPackSub(&subObjects[i], slice, slices[i]);
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
    std::vector<std::vector<VPackSlice>> toInsert;
    std::vector<VPackSlice> sliceStack;

    buildIndexValues(slice, 0, toInsert, sliceStack);

    if (!toInsert.empty()) {
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
        TRI_vpack_sub_t* subObjects = element->subObjects();

        for (size_t j = 0; j < n; ++j) {
          TRI_FillVPackSub(&subObjects[j], slice, info[j]);
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

std::vector<VPackSlice> PathBasedIndex::buildIndexValue(
    VPackSlice const documentSlice) {
  size_t const n = _paths.size();

  std::vector<VPackSlice> result;
  for (size_t i = 0; i < n; ++i) {
    TRI_ASSERT(!_paths[i].empty());

    VPackSlice slice = documentSlice.get(_paths[i]);
    if (slice.isNone() || slice.isNull()) {
      // attribute not found
      if (_sparse) {
        // if sparse we do not have to index, this is indicated by result
        // being shorter than n
        result.clear();
        break;
      }
      // null, note that this will be copied later!
      result.emplace_back(arangodb::basics::VelocyPackHelper::NullValue());
    } else {
      result.emplace_back(slice);
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to create a set of index combinations to insert
////////////////////////////////////////////////////////////////////////////////

void PathBasedIndex::buildIndexValues(
    VPackSlice const document, size_t level,
    std::vector<std::vector<VPackSlice>>& toInsert,
    std::vector<VPackSlice>& sliceStack) {
  // Invariant: level == sliceStack.size()

  // Stop the recursion:
  if (level == _paths.size()) {
    toInsert.push_back(sliceStack);
    return;
  }

  if (_expanding[level] == -1) {   // the trivial, non-expanding case
    VPackSlice slice = document.get(_paths[level]);
    if (slice.isNone() || slice.isNull()) {
      if (_sparse) {
        return;
      }
      slice = arangodb::basics::VelocyPackHelper::NullValue();
    }
    sliceStack.push_back(slice);
    buildIndexValues(document, level+1, toInsert, sliceStack);
    sliceStack.pop_back();
    return;
  }

  // Finally, the complex case, where we have to expand one entry.
  // Note again that at most one step in the attribute path can be
  // an array step. Furthermore, if _allowPartialIndex is true and
  // anything goes wrong with this attribute path, we have to bottom out
  // with None values to be able to use the index for a prefix match.

  // Trivial case to bottom out with Illegal types.
  VPackSlice illegalSlice = arangodb::basics::VelocyPackHelper::IllegalValue();

  auto finishWithNones = [&]() -> void {
    if (!_allowPartialIndex || level == 0) {
      return;
    }
    for (size_t i = level; i < _paths.size(); i++) {
      sliceStack.push_back(illegalSlice);
    }
    toInsert.push_back(sliceStack);
    for (size_t i = level; i < _paths.size(); i++) {
      sliceStack.pop_back();
    }
  };
  size_t const n = _paths[level].size();
  // We have 0 <= _expanding[level] < n.
  VPackSlice current(document);
  for (size_t i = 0; i <= static_cast<size_t>(_expanding[level]); i++) {
    if (!current.isObject()) {
      finishWithNones();
      return;
    }
    current = current.get(_paths[level][i]);
    if (current.isNone()) {
      finishWithNones();
      return;
    }
  }
  // Now the expansion:
  if (!current.isArray() || current.length() == 0) {
    finishWithNones();
    return;
  }

  std::unordered_set<VPackSlice,
                     arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
              arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.push_back(something);
      buildIndexValues(document, level + 1, toInsert, sliceStack);
      sliceStack.pop_back();
    }
  };
  for (auto const& member : VPackArrayIterator(current)) {
    VPackSlice current2(member);
    bool doneNull = false;
    for (size_t i = _expanding[level] + 1; i < n; i++) {
      if (!current2.isObject()) {
        if (!_sparse) {
          moveOn(arangodb::basics::VelocyPackHelper::NullValue());
        }
        doneNull = true;
        break;
      }
      current2 = current2.get(_paths[level][i]);
      if (current2.isNone()) {
        if (!_sparse) {
          moveOn(arangodb::basics::VelocyPackHelper::NullValue());
        }
        doneNull = true;
        break;
      }
    }
    if (!doneNull) {
      moveOn(current2);
    }
    // Finally, if, because of sparsity, we have not inserted anything by now,
    // we need to play the above trick with None because of the above mentioned
    // reasons:
    if (seen.empty()) {
      finishWithNones();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to transform AttributeNames into strings.
////////////////////////////////////////////////////////////////////////////////

void PathBasedIndex::fillPaths(std::vector<std::vector<std::string>>& paths,
                               std::vector<int>& expanding) {
  paths.clear();
  expanding.clear();
  for (std::vector<arangodb::basics::AttributeName> const& list : _fields) {
    paths.emplace_back();
    std::vector<std::string>& interior(paths.back());
    int expands = -1;
    int count = 0;
    for (auto const& att : list) {
      interior.emplace_back(att.name);
      if (att.shouldExpand) {
        expands = count;
      }
      ++count;
    }
    expanding.emplace_back(expands);
  }
}
