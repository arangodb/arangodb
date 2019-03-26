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

#include "MMFilesPathBasedIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesIndexElement.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief create the index
MMFilesPathBasedIndex::MMFilesPathBasedIndex(TRI_idx_iid_t iid,
                                             arangodb::LogicalCollection& collection,
                                             arangodb::velocypack::Slice const& info,
                                             size_t baseSize, bool allowPartialIndex)
    : MMFilesIndex(iid, collection, info),
      _deduplicate(
          arangodb::basics::VelocyPackHelper::getBooleanValue(info,
                                                              "deduplicate", true)),
      _allowPartialIndex(allowPartialIndex) {
  TRI_ASSERT(!_fields.empty());

  TRI_ASSERT(iid != 0);

  fillPaths(_paths, _expanding);

  TRI_ASSERT(baseSize > 0);

  _allocator.reset(new FixedSizeAllocator(
      baseSize + sizeof(MMFilesIndexElementValue) * numPaths()));
}

/// @brief destroy the index
MMFilesPathBasedIndex::~MMFilesPathBasedIndex() { _allocator->deallocateAll(); }

void MMFilesPathBasedIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("memory", VPackValue(memory()));
}

/// @brief return a VelocyPack representation of the index
void MMFilesPathBasedIndex::toVelocyPack(VPackBuilder& builder,
                                         std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  builder.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(_unique));
  builder.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(_sparse));
  builder.add("deduplicate", VPackValue(_deduplicate));
  builder.close();
}

/// @brief helper function to insert a document into any index type
template <typename T>
int MMFilesPathBasedIndex::fillElement(std::vector<T*>& elements,
                                       LocalDocumentId const& documentId,
                                       VPackSlice const& doc) {
  if (doc.isNone()) {
    LOG_TOPIC("e5ac9", ERR, arangodb::Logger::ENGINES)
        << "encountered invalid marker with slice of type None";
    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalSlice") { return TRI_ERROR_INTERNAL; }

  size_t const n = _paths.size();

  if (!_useExpansion) {
    // fast path for inserts... no array elements used
    auto slices = buildIndexValue(doc);

    if (slices.size() == n) {
      // if slices.size() != n, then the value is not inserted into the index
      // because of index sparsity!
      T* element = static_cast<T*>(_allocator->allocate());
      TRI_ASSERT(element != nullptr);
      element = T::initialize(element, documentId, slices);

      if (element == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
      TRI_IF_FAILURE("FillElementOOM") {
        // clean up manually
        _allocator->deallocate(element);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      try {
        TRI_IF_FAILURE("FillElementOOM2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }

        elements.emplace_back(element);
      } catch (...) {
        _allocator->deallocate(element);
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    // other path for handling array elements, too
    std::vector<std::vector<std::pair<VPackSlice, uint32_t>>> toInsert;
    std::vector<std::pair<VPackSlice, uint32_t>> sliceStack;

    try {
      buildIndexValues(doc, 0, toInsert, sliceStack);
    } catch (basics::Exception const& ex) {
      return ex.code();
    } catch (...) {
      return TRI_ERROR_INTERNAL;
    }

    if (!toInsert.empty()) {
      elements.reserve(toInsert.size());

      for (auto& info : toInsert) {
        TRI_ASSERT(info.size() == n);
        T* element = static_cast<T*>(_allocator->allocate());
        TRI_ASSERT(element != nullptr);
        element = T::initialize(element, documentId, info);

        if (element == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }
        TRI_IF_FAILURE("FillElementOOM") {
          // clean up manually
          _allocator->deallocate(element);
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        try {
          TRI_IF_FAILURE("FillElementOOM2") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          elements.emplace_back(element);
        } catch (...) {
          _allocator->deallocate(element);
          return TRI_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief helper function to create the sole index value insert
std::vector<std::pair<VPackSlice, uint32_t>> MMFilesPathBasedIndex::buildIndexValue(VPackSlice const documentSlice) {
  size_t const n = _paths.size();

  std::vector<std::pair<VPackSlice, uint32_t>> result;
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
      result.emplace_back(arangodb::velocypack::Slice::nullSlice(),
                          0);  // fake offset 0
    } else {
      result.emplace_back(slice, static_cast<uint32_t>(slice.start() -
                                                       documentSlice.start()));
    }
  }
  return result;
}

/// @brief helper function to create a set of index combinations to insert
void MMFilesPathBasedIndex::buildIndexValues(
    VPackSlice const document, size_t level,
    std::vector<std::vector<std::pair<VPackSlice, uint32_t>>>& toInsert,
    std::vector<std::pair<VPackSlice, uint32_t>>& sliceStack) {
  // Invariant: level == sliceStack.size()

  // Stop the recursion:
  if (level == _paths.size()) {
    toInsert.push_back(sliceStack);
    return;
  }

  if (_expanding[level] == -1) {  // the trivial, non-expanding case
    VPackSlice slice = document.get(_paths[level]);
    if (slice.isNone() || slice.isNull()) {
      if (_sparse) {
        return;
      }
      sliceStack.emplace_back(arangodb::velocypack::Slice::nullSlice(), 0);
    } else {
      sliceStack.emplace_back(slice,
                              static_cast<uint32_t>(slice.start() - document.start()));
    }
    buildIndexValues(document, level + 1, toInsert, sliceStack);
    sliceStack.pop_back();
    return;
  }

  // Finally, the complex case, where we have to expand one entry.
  // Note again that at most one step in the attribute path can be
  // an array step. Furthermore, if _allowPartialIndex is true and
  // anything goes wrong with this attribute path, we have to bottom out
  // with None values to be able to use the index for a prefix match.

  // Trivial case to bottom out with Illegal types.
  VPackSlice illegalSlice = arangodb::velocypack::Slice::illegalSlice();

  auto finishWithNones = [&]() -> void {
    if (!_allowPartialIndex || level == 0) {
      return;
    }
    for (size_t i = level; i < _paths.size(); i++) {
      sliceStack.emplace_back(illegalSlice, 0);
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

  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash, arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
           arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.emplace_back(something, static_cast<uint32_t>(something.start() -
                                                               document.start()));
      buildIndexValues(document, level + 1, toInsert, sliceStack);
      sliceStack.pop_back();
    } else if (_unique && !_deduplicate) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    }
  };
  for (auto const& member : VPackArrayIterator(current)) {
    VPackSlice current2(member);
    bool doneNull = false;
    for (size_t i = _expanding[level] + 1; i < n; i++) {
      if (!current2.isObject()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
        }
        doneNull = true;
        break;
      }
      current2 = current2.get(_paths[level][i]);
      if (current2.isNone()) {
        if (!_sparse) {
          moveOn(arangodb::velocypack::Slice::nullSlice());
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

/// @brief helper function to transform AttributeNames into strings.
void MMFilesPathBasedIndex::fillPaths(std::vector<std::vector<std::string>>& paths,
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

// template instanciations for fillElement
template int MMFilesPathBasedIndex::fillElement(std::vector<MMFilesHashIndexElement*>&,
                                                LocalDocumentId const&,
                                                VPackSlice const& doc);

template int MMFilesPathBasedIndex::fillElement(std::vector<MMFilesSkiplistIndexElement*>&,
                                                LocalDocumentId const&,
                                                VPackSlice const& doc);
