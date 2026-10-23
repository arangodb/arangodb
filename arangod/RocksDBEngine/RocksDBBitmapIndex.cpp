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
#include <limits>
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

/// Iterates the shards that may hold results of one part of a condition, in
/// ascending order. seek() targets never decrease.
class ShardNode {
 public:
  virtual ~ShardNode() = default;
  /// position on the first shard >= target that may hold results; false if
  /// there is none
  virtual bool seek(std::uint64_t target) = 0;
  virtual std::uint64_t shard() const = 0;
  /// matching offsets in the current shard
  virtual roaring::Roaring bitmap() = 0;
  /// estimated number of matching documents
  virtual std::int64_t cost() const = 0;
};

class TermNode final : public ShardNode {
 public:
  TermNode(std::string keyPrefix, rocksdb::Iterator& it)
      : _keyPrefix(std::move(keyPrefix)), _it(it) {
    // the count key sorts directly before the term's shard keys
    _it.Seek(_keyPrefix);
    if (_it.Valid() && _it.key() == rocksdb::Slice(_keyPrefix)) {
      rocksdb::Slice value = _it.value();
      _count = bitmap_index::decodeCount({value.data(), value.size()});
      _it.Next();
    }
    if (_count <= 0) {
      // no document contains this element
      _exhausted = true;
    } else {
      position();
    }
  }

  bool seek(std::uint64_t target) override {
    if (_exhausted) {
      return false;
    }
    if (_valid && _shard >= target) {
      return true;
    }
    std::string key = _keyPrefix;
    bitmap_index::appendShard(key, target);
    _it.Seek(key);
    position();
    _exhausted = !_valid;
    return _valid;
  }

  std::uint64_t shard() const override { return _shard; }

  roaring::Roaring bitmap() override {
    TRI_ASSERT(_valid);
    rocksdb::Slice value = _it.value();
    return bitmap_index::decodeBitmap({value.data(), value.size()});
  }

  std::int64_t cost() const override { return _count; }

 private:
  /// position on the shard key the iterator points to, if any
  void position() {
    _valid = false;
    if (!_it.Valid()) {
      rocksutils::checkIteratorStatus(_it);
      return;
    }
    rocksdb::Slice key = _it.key();
    if (key.size() != _keyPrefix.size() + bitmap_index::kShardBytes ||
        !key.starts_with(rocksdb::Slice(_keyPrefix))) {
      return;
    }
    _shard = bitmap_index::readShard(key.data() + _keyPrefix.size());
    _valid = true;
  }

  /// objectId | prefix values | term
  std::string const _keyPrefix;
  rocksdb::Iterator& _it;
  std::int64_t _count = 0;
  std::uint64_t _shard = 0;
  bool _valid = false;
  bool _exhausted = false;
};

/// intersection of the required children, minus the excluded ones
class AndNode final : public ShardNode {
 public:
  AndNode(std::vector<std::unique_ptr<ShardNode>> required,
          std::vector<std::unique_ptr<ShardNode>> excluded)
      : _required(std::move(required)), _excluded(std::move(excluded)) {
    TRI_ASSERT(!_required.empty());
    // the rarest child drives the leapfrog
    std::sort(_required.begin(), _required.end(),
              [](auto const& a, auto const& b) { return a->cost() < b->cost(); });
  }

  bool seek(std::uint64_t target) override {
    std::size_t const n = _required.size();
    std::size_t agreed = 0;
    std::size_t i = 0;
    while (agreed < n) {
      if (!_required[i]->seek(target)) {
        return false;
      }
      if (std::uint64_t s = _required[i]->shard(); s > target) {
        target = s;
        agreed = 1;
      } else {
        ++agreed;
      }
      i = (i + 1) % n;
    }
    _shard = target;
    return true;
  }

  std::uint64_t shard() const override { return _shard; }

  roaring::Roaring bitmap() override {
    roaring::Roaring result = _required[0]->bitmap();
    for (std::size_t i = 1; i < _required.size() && !result.isEmpty(); ++i) {
      result &= _required[i]->bitmap();
    }
    for (auto& excluded : _excluded) {
      if (result.isEmpty()) {
        break;
      }
      if (excluded->seek(_shard) && excluded->shard() == _shard) {
        result -= excluded->bitmap();
      }
    }
    return result;
  }

  std::int64_t cost() const override { return _required[0]->cost(); }

 private:
  std::vector<std::unique_ptr<ShardNode>> _required;
  std::vector<std::unique_ptr<ShardNode>> _excluded;
  std::uint64_t _shard = 0;
};

/// union of the children
class OrNode final : public ShardNode {
 public:
  explicit OrNode(std::vector<std::unique_ptr<ShardNode>> children)
      : _children(std::move(children)) {
    for (auto const& child : _children) {
      _cost += child->cost();
    }
  }

  bool seek(std::uint64_t target) override {
    bool found = false;
    _shard = std::numeric_limits<std::uint64_t>::max();
    for (auto& child : _children) {
      if (child->seek(target)) {
        found = true;
        _shard = std::min(_shard, child->shard());
      }
    }
    return found;
  }

  std::uint64_t shard() const override { return _shard; }

  roaring::Roaring bitmap() override {
    roaring::Roaring result;
    for (auto& child : _children) {
      if (child->seek(_shard) && child->shard() == _shard) {
        result |= child->bitmap();
      }
    }
    return result;
  }

  std::int64_t cost() const override { return _cost; }

 private:
  std::vector<std::unique_ptr<ShardNode>> _children;
  std::int64_t _cost = 0;
  std::uint64_t _shard = 0;
};

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
  void setup(aql::AstNode const* node) {
    auto parts = bitmap_index::matchCondition(*_index, node, _reference);
    if (!parts.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported bitmap index condition");
    }

    _usable = true;
    _prefix.clear();
    for (auto const* value : parts->prefixValues) {
      velocypack::Builder builder;
      value->toVelocyPackValue(builder);
      if (!bitmap_index::appendNormalized(_prefix, builder.slice())) {
        // non-scalar prefix values are never indexed
        _usable = false;
      }
    }
    _expr = std::move(parts->expr);
    restart();
  }

  /// RocksDB iterators are kept across rearms, only the keys change
  rocksdb::Iterator& nextIterator() {
    if (_iteratorsUsed == _iterators.size()) {
      RocksDBTransactionMethods* mthds =
          RocksDBTransactionState::toMethods(_trx, _collection->id());
      _iterators.push_back(mthds->NewIterator(
          _index->columnFamily(), [this](ReadOptions& options) {
            options.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
          }));
    }
    return *_iterators[_iteratorsUsed++];
  }

  std::unique_ptr<ShardNode> build(bitmap_index::TermExpr const& expr) {
    switch (expr.type) {
      case bitmap_index::TermExpr::Type::Term: {
        velocypack::Builder builder;
        expr.value->toVelocyPackValue(builder);
        std::string body = _prefix;
        bool ok = bitmap_index::appendNormalized(body, builder.slice());
        TRI_ASSERT(ok);
        RocksDBKey key;
        key.constructBitmapIndexValue(_index->objectId(), body);
        return std::make_unique<TermNode>(key.string().ToString(),
                                          nextIterator());
      }
      case bitmap_index::TermExpr::Type::And: {
        std::vector<std::unique_ptr<ShardNode>> required;
        std::vector<std::unique_ptr<ShardNode>> excluded;
        for (auto const& child : expr.children) {
          required.push_back(build(child));
        }
        for (auto const& child : expr.excluded) {
          excluded.push_back(build(child));
        }
        return std::make_unique<AndNode>(std::move(required),
                                         std::move(excluded));
      }
      case bitmap_index::TermExpr::Type::Or: {
        std::vector<std::unique_ptr<ShardNode>> children;
        for (auto const& child : expr.children) {
          children.push_back(build(child));
        }
        return std::make_unique<OrNode>(std::move(children));
      }
    }
    TRI_ASSERT(false);
    return nullptr;
  }

  void restart() {
    _buffer.clear();
    _pos = 0;
    _nextShard = 0;
    _root.reset();
    _iteratorsUsed = 0;
    _exhausted = !_usable;
    if (!_exhausted) {
      _root = build(_expr);
    }
  }

  bool fillBuffer() {
    while (_root->seek(_nextShard)) {
      std::uint64_t const shard = _root->shard();
      roaring::Roaring result = _root->bitmap();
      _nextShard = shard + 1;
      if (!result.isEmpty()) {
        _buffer.resize(result.cardinality());
        result.toUint32Array(_buffer.data());
        _bufferShard = shard;
        _pos = 0;
        return true;
      }
    }
    return false;
  }

  RocksDBBitmapIndex const* _index;
  aql::Variable const* _reference;
  std::string _prefix;
  bitmap_index::TermExpr _expr;
  std::unique_ptr<ShardNode> _root;
  std::vector<std::unique_ptr<rocksdb::Iterator>> _iterators;
  std::size_t _iteratorsUsed = 0;
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
