////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBBitmapIndex.h"

#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Indexes/BitmapIndexMatcher.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/iterator.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <algorithm>
#include <set>

using namespace arangodb;

namespace {

constexpr std::string_view kShardBitsAttribute = "shardBits";

velocypack::Slice extractField(
    velocypack::Slice doc, std::vector<basics::AttributeName> const& path) {
  if (path.size() == 1) {
    if (path[0].name == StaticStrings::FromString) {
      return transaction::helpers::extractFromFromDocument(doc);
    }
    if (path[0].name == StaticStrings::ToString) {
      return transaction::helpers::extractToFromDocument(doc);
    }
  }
  velocypack::Slice current = doc;
  for (auto const& part : path) {
    if (!current.isObject()) {
      return velocypack::Slice::noneSlice();
    }
    current = current.get(part.name);
  }
  return current;
}

/// append the normalized encoding of each distinct scalar in `value` (an
/// array, or a single scalar) to `out`
void collectTerms(velocypack::Slice value, std::set<std::string>& out) {
  auto add = [&out](velocypack::Slice element) {
    std::string term;
    if (bitmap_index::appendNormalized(term, element)) {
      out.emplace(std::move(term));
    }
  };
  if (value.isArray()) {
    for (velocypack::Slice element : velocypack::ArrayIterator(value)) {
      add(element);
    }
  } else {
    add(value);
  }
}

class RocksDBBitmapIndexIterator final : public IndexIterator {
 public:
  RocksDBBitmapIndexIterator(LogicalCollection* collection,
                             transaction::Methods* trx,
                             RocksDBBitmapIndex const* index,
                             aql::AstNode const* node,
                             aql::Variable const* reference,
                             ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _reference(reference) {
    setup(node);
  }

  std::string_view typeName() const noexcept override {
    return "rocksdb-bitmap-index-iterator";
  }

  bool canRearm() const override { return true; }

 protected:
  bool rearmImpl(aql::AstNode const* node, aql::Variable const* /*variable*/,
                 IndexIteratorOptions const& /*opts*/) override {
    setup(node);
    // an empty result is fine; returning false would make traversals replace
    // this iterator with one that cannot be rearmed for the next vertex
    return true;
  }

  void resetImpl() override { restart(); }

  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    while (limit > 0) {
      if (_pos < _buffer.size()) {
        callback(LocalDocumentId::create(
            bitmap_index::makeId(_bufferShard, _buffer[_pos])));
        ++_pos;
        --limit;
        continue;
      }
      if (_exhausted || !fillBuffer()) {
        _exhausted = true;
        return false;
      }
    }
    return _pos < _buffer.size() || !_exhausted;
  }

 private:
  struct Cursor {
    /// objectId | prefix values | term
    std::string keyPrefix;
    std::unique_ptr<rocksdb::Iterator> it;
    std::int64_t count = 0;
    std::uint64_t shard = 0;
    bool valid = false;
  };

  void setup(aql::AstNode const* node) {
    auto parts = bitmap_index::matchCondition(*_index, node, _reference);
    if (!parts.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported bitmap index condition");
    }

    bool usable = true;
    std::string prefix;
    for (auto const* value : parts->prefixValues) {
      velocypack::Builder builder;
      value->toVelocyPackValue(builder);
      if (!bitmap_index::appendNormalized(prefix, builder.slice())) {
        // non-scalar prefix values are never indexed
        usable = false;
      }
    }

    std::set<std::string> terms;
    for (auto const* value : parts->termValues) {
      velocypack::Builder builder;
      value->toVelocyPackValue(builder);
      collectTerms(builder.slice(), terms);
    }
    if (terms.empty()) {
      usable = false;
    }

    // iterators are kept across rearms; only the keys change
    _cursors.resize(terms.size());
    std::size_t i = 0;
    for (auto const& term : terms) {
      RocksDBKey key;
      key.constructBitmapIndexValue(_index->objectId(), prefix + term);
      _cursors[i++].keyPrefix = key.string().ToString();
    }
    _usable = usable;
    restart();
  }

  rocksdb::Iterator& iteratorFor(Cursor& cursor) {
    if (cursor.it == nullptr) {
      RocksDBTransactionMethods* mthds =
          RocksDBTransactionState::toMethods(_trx, _collection->id());
      cursor.it = mthds->NewIterator(
          _index->columnFamily(), [this](ReadOptions& options) {
            options.readOwnWrites =
                canReadOwnWrites() == ReadOwnWrites::yes;
          });
    }
    return *cursor.it;
  }

  /// position the cursor on the shard key the iterator points to, if any
  void position(Cursor& cursor) {
    rocksdb::Iterator& it = *cursor.it;
    cursor.valid = false;
    if (!it.Valid()) {
      rocksutils::checkIteratorStatus(it);
      return;
    }
    rocksdb::Slice key = it.key();
    std::string_view const& kp = cursor.keyPrefix;
    if (key.size() != kp.size() + bitmap_index::kShardBytes ||
        !key.starts_with(rocksdb::Slice(kp.data(), kp.size()))) {
      return;
    }
    cursor.shard = bitmap_index::readShard(key.data() + kp.size());
    cursor.valid = true;
  }

  void restart() {
    _buffer.clear();
    _pos = 0;
    _nextShard = 0;
    _exhausted = !_usable;
    if (_exhausted) {
      return;
    }

    for (auto& cursor : _cursors) {
      rocksdb::Iterator& it = iteratorFor(cursor);
      // the count key sorts directly before the term's shard keys
      it.Seek(cursor.keyPrefix);
      cursor.count = 0;
      if (it.Valid() && it.key() == rocksdb::Slice(cursor.keyPrefix)) {
        rocksdb::Slice value = it.value();
        cursor.count = bitmap_index::decodeCount({value.data(), value.size()});
        it.Next();
      }
      if (cursor.count <= 0) {
        // no document contains this element
        _exhausted = true;
        return;
      }
      position(cursor);
    }

    // rarest element first: it drives the intersection
    std::sort(_cursors.begin(), _cursors.end(),
              [](Cursor const& a, Cursor const& b) { return a.count < b.count; });
  }

  bool seek(Cursor& cursor, std::uint64_t target) {
    if (cursor.valid && cursor.shard >= target) {
      return true;
    }
    std::string key = cursor.keyPrefix;
    bitmap_index::appendShard(key, target);
    cursor.it->Seek(key);
    position(cursor);
    return cursor.valid;
  }

  /// leapfrog over the shard numbers of all cursors until all agree on a
  /// shard with a non-empty intersection
  bool fillBuffer() {
    std::size_t const n = _cursors.size();
    while (true) {
      std::uint64_t target = _nextShard;
      std::size_t agreed = 0;
      std::size_t i = 0;
      while (agreed < n) {
        Cursor& cursor = _cursors[i];
        if (!seek(cursor, target)) {
          return false;
        }
        if (cursor.shard > target) {
          target = cursor.shard;
          agreed = 1;
        } else {
          ++agreed;
        }
        i = (i + 1) % n;
      }

      auto valueOf = [](Cursor const& cursor) {
        rocksdb::Slice value = cursor.it->value();
        return std::string_view{value.data(), value.size()};
      };
      roaring::Roaring result = bitmap_index::decodeBitmap(valueOf(_cursors[0]));
      for (std::size_t j = 1; j < n && !result.isEmpty(); ++j) {
        result &= bitmap_index::decodeBitmap(valueOf(_cursors[j]));
      }

      _nextShard = target + 1;
      if (!result.isEmpty()) {
        _buffer.resize(result.cardinality());
        result.toUint32Array(_buffer.data());
        _bufferShard = target;
        _pos = 0;
        return true;
      }
    }
  }

  RocksDBBitmapIndex const* _index;
  aql::Variable const* _reference;
  std::vector<Cursor> _cursors;
  std::vector<std::uint32_t> _buffer;
  std::size_t _pos = 0;
  std::uint64_t _bufferShard = 0;
  std::uint64_t _nextShard = 0;
  bool _usable = false;
  bool _exhausted = true;
};

}  // namespace

RocksDBBitmapIndex::RocksDBBitmapIndex(IndexId iid,
                                       LogicalCollection& collection,
                                       velocypack::Slice info)
    : RocksDBIndex(iid, collection, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::BitmapIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   collection.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(iid.isSet());
  _unique = false;
  _sparse = false;
  if (_fields.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "bitmap index definition needs at least one attribute");
  }
  if (velocypack::Slice bits = info.get(kShardBitsAttribute);
      bits.isNumber() &&
      bits.getNumber<unsigned>() != bitmap_index::kShardBits) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "bitmap index was built with " + std::to_string(bits.getNumber<unsigned>()) +
            " shard bits, but this build uses " +
            std::to_string(bitmap_index::kShardBits));
  }
}

void RocksDBBitmapIndex::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(kShardBitsAttribute, velocypack::Value(bitmap_index::kShardBits));
  builder.close();
}

Index::FilterCosts RocksDBBitmapIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& /*allIndexes*/,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  return bitmap_index::supportsFilterCondition(*this, node, reference,
                                               itemsInIndex);
}

aql::AstNode* RocksDBBitmapIndex::specializeCondition(
    transaction::Methods& /*trx*/, aql::AstNode* node,
    aql::Variable const* reference) const {
  return bitmap_index::specializeCondition(*this, node, reference);
}

std::unique_ptr<IndexIterator> RocksDBBitmapIndex::iteratorForCondition(
    ResourceMonitor& /*monitor*/, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& /*opts*/, ReadOwnWrites readOwnWrites,
    int /*mutableConditionIdx*/) {
  return std::make_unique<RocksDBBitmapIndexIterator>(
      &_collection, trx, this, node, reference, readOwnWrites);
}

bool RocksDBBitmapIndex::encodePrefix(velocypack::Slice doc,
                                      std::string& out) const {
  for (std::size_t i = 0; i + 1 < _fields.size(); ++i) {
    velocypack::Slice value = extractField(doc, _fields[i]);
    if (value.isNone()) {
      value = velocypack::Slice::nullSlice();
    }
    if (!bitmap_index::appendNormalized(out, value)) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> RocksDBBitmapIndex::encodeTerms(
    velocypack::Slice doc) const {
  std::set<std::string> terms;
  velocypack::Slice value = extractField(doc, _fields.back());
  if (value.isArray()) {
    collectTerms(value, terms);
  }
  return {terms.begin(), terms.end()};
}

Result RocksDBBitmapIndex::insert(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/,
                                  bool /*performChecks*/) {
  return write(methods, documentId, doc, /*isInsert*/ true);
}

Result RocksDBBitmapIndex::remove(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/) {
  return write(methods, documentId, doc, /*isInsert*/ false);
}

Result RocksDBBitmapIndex::write(RocksDBMethods* methods,
                                 LocalDocumentId documentId,
                                 velocypack::Slice doc, bool isInsert) {
  std::string prefix;
  if (!encodePrefix(doc, prefix)) {
    return {};
  }
  std::vector<std::string> terms = encodeTerms(doc);
  if (terms.empty()) {
    return {};
  }

  std::uint64_t const id = documentId.id();
  roaring::Roaring single;
  single.add(bitmap_index::offsetOf(id));
  roaring::Roaring none;
  std::string const delta = isInsert ? bitmap_index::encodeDelta(single, none)
                                     : bitmap_index::encodeDelta(none, single);
  std::string const countDelta = bitmap_index::encodeCount(
      bitmap_index::ValueTag::CountDelta, isInsert ? 1 : -1);

  Result res;
  RocksDBKey key;
  std::string body;
  for (auto const& term : terms) {
    body = prefix;
    body.append(term);
    key.constructBitmapIndexValue(objectId(), body);
    rocksdb::Status s = methods->MergeUntracked(_cf, key, countDelta);
    if (s.ok()) {
      bitmap_index::appendShard(body, bitmap_index::shardOf(id));
      key.constructBitmapIndexValue(objectId(), body);
      s = methods->MergeUntracked(_cf, key, delta);
    }
    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }
  return res;
}
