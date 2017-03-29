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

#include "RocksDBPathBasedIndex.h"
#include "Aql/AstNode.h"
#include "Basics/FixedSizeAllocator.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "Transaction/Helpers.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief the _key attribute, which, when used in an index, will implictly make
/// it unique
static std::vector<arangodb::basics::AttributeName> const KeyAttribute{
    arangodb::basics::AttributeName("_key", false)};

/// @brief create the index
RocksDBPathBasedIndex::RocksDBPathBasedIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    VPackSlice const& info, size_t baseSize, bool allowPartialIndex)
    : RocksDBIndex(iid, collection, info),
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

  //_allocator.reset(new FixedSizeAllocator(baseSize +
  // sizeof(MMFilesIndexElementValue) * numPaths()));
}

/// @brief destroy the index
RocksDBPathBasedIndex::~RocksDBPathBasedIndex() {
  //_allocator->deallocateAll();
}

/// @brief whether or not the index is implicitly unique
/// this can be the case if the index is not declared as unique, but contains a
/// unique attribute such as _key
bool RocksDBPathBasedIndex::implicitlyUnique() const {
  if (_unique) {
    // a unique index is always unique
    return true;
  }
  if (_useExpansion) {
    // when an expansion such as a[*] is used, the index may not be unique, even
    // if it contains attributes that are guaranteed to be unique
    return false;
  }

  for (auto const& it : _fields) {
    // if _key is contained in the index fields definition, then the index is
    // implicitly unique
    if (it == KeyAttribute) {
      return true;
    }
  }

  // _key not contained
  return false;
}

/// @brief helper function to insert a document into any index type
/// Should result in an elements vector filled with the new index entries
/// uses the _unique field to determine the kind of key structure
int RocksDBPathBasedIndex::fillElement(
    transaction::Methods* trx, TRI_voc_rid_t revisionId, VPackSlice const& doc,
    std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements) {
  if (doc.isNone()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "encountered invalid marker with slice of type None";
    return TRI_ERROR_INTERNAL;
  }

  TRI_IF_FAILURE("FillElementIllegalSlice") { return TRI_ERROR_INTERNAL; }

  if (!_useExpansion) {
    // fast path for inserts... no array elements used

    transaction::BuilderLeaser indexVals(trx);
    indexVals->openArray();

    size_t const n = _paths.size();
    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(!_paths[i].empty());

      VPackSlice slice = doc.get(_paths[i]);
      if (slice.isNone() || slice.isNull()) {
        // attribute not found
        if (_sparse) {
          // if sparse we do not have to index, this is indicated by result
          // being shorter than n
          return TRI_ERROR_NO_ERROR;
        }
        // null, note that this will be copied later!
        indexVals->add(VPackSlice::nullSlice());
      } else {
        indexVals->add(slice);
      }
    }
    indexVals->close();

    StringRef key(doc.get(StaticStrings::KeyString));
    if (_unique) {
      // Unique VPack index values are stored as follows:
      // - Key: 7 + 8-byte object ID of index + VPack array with index value(s)
      // + separator (NUL) byte
      // - Value: primary key
      elements.emplace_back(
          RocksDBKey::UniqueIndexValue(_objectId, indexVals->slice()),
          RocksDBValue::UniqueIndexValue(key));
    } else {
      // Non-unique VPack index values are stored as follows:
      // - Key: 6 + 8-byte object ID of index + VPack array with index value(s)
      // + separator (NUL) byte + primary key
      // - Value: empty
      elements.emplace_back(
          RocksDBKey::IndexValue(_objectId, key, indexVals->slice()),
          RocksDBValue::IndexValue());
    }
  } else {
    // other path for handling array elements, too
    std::vector<VPackSlice> sliceStack;

    transaction::BuilderLeaser indexVals(trx);

    buildIndexValues(doc, 0, elements, sliceStack);
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBPathBasedIndex::addIndexValue(
    VPackSlice const& document,
    std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements,
    std::vector<VPackSlice>& sliceStack) {
  // TODO maybe use leaded Builder from transaction.
  VPackBuilder b;
  b.openArray();
  for (VPackSlice const& s : sliceStack) {
    b.add(s);
  }
  b.close();

  StringRef key(document.get(StaticStrings::KeyString));
  if (_unique) {
    // Unique VPack index values are stored as follows:
    // - Key: 7 + 8-byte object ID of index + VPack array with index value(s)
    // - Value: primary key
    elements.emplace_back(RocksDBKey::UniqueIndexValue(_objectId, b.slice()),
                          RocksDBValue::UniqueIndexValue(key));
  } else {
    // Non-unique VPack index values are stored as follows:
    // - Key: 6 + 8-byte object ID of index + VPack array with index value(s)
    // + primary key
    // - Value: empty
    elements.emplace_back(
        RocksDBKey::IndexValue(_objectId, StringRef(key), b.slice()),
        RocksDBValue::IndexValue());
  }
}

/// @brief helper function to create a set of index combinations to insert
void RocksDBPathBasedIndex::buildIndexValues(
    VPackSlice const document, size_t level,
    std::vector<std::pair<RocksDBKey, RocksDBValue>>& elements,
    std::vector<VPackSlice>& sliceStack) {
  // Invariant: level == sliceStack.size()

  // Stop the recursion:
  if (level == _paths.size()) {
    addIndexValue(document, elements, sliceStack);
    return;
  }

  if (_expanding[level] == -1) {  // the trivial, non-expanding case
    VPackSlice slice = document.get(_paths[level]);
    if (slice.isNone() || slice.isNull()) {
      if (_sparse) {
        return;
      }
      sliceStack.emplace_back(arangodb::basics::VelocyPackHelper::NullValue());
    } else {
      sliceStack.emplace_back(slice);
    }
    buildIndexValues(document, level + 1, elements, sliceStack);
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
      sliceStack.emplace_back(illegalSlice);
    }
    addIndexValue(document.get(StaticStrings::KeyString), elements, sliceStack);
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

  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash,
                     arangodb::basics::VelocyPackHelper::VPackEqual>
      seen(2, arangodb::basics::VelocyPackHelper::VPackHash(),
           arangodb::basics::VelocyPackHelper::VPackEqual());

  auto moveOn = [&](VPackSlice something) -> void {
    auto it = seen.find(something);
    if (it == seen.end()) {
      seen.insert(something);
      sliceStack.emplace_back(something);
      buildIndexValues(document, level + 1, elements, sliceStack);
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

/// @brief helper function to transform AttributeNames into strings.
void RocksDBPathBasedIndex::fillPaths(
    std::vector<std::vector<std::string>>& paths, std::vector<int>& expanding) {
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
