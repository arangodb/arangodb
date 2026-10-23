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
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"
#include "Indexes/BitmapIndexMatcher.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
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
#include <optional>
#include <unordered_set>

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

using SliceSet = std::unordered_set<velocypack::Slice,
                                    basics::VelocyPackHelper::VPackHash,
                                    basics::VelocyPackHelper::VPackEqual>;

/// key of a count (without shard) or of a bitmap (with shard):
/// objectId | [prefix values..., element] | shard
std::string makeKey(std::uint64_t objectId, velocypack::Slice prefixValues,
                    velocypack::Slice element) {
  velocypack::Builder body;
  body.openArray();
  for (velocypack::Slice value : velocypack::ArrayIterator(prefixValues)) {
    body.add(value);
  }
  body.add(element);
  body.close();
  RocksDBKey key;
  key.constructBitmapIndexValue(
      objectId, {reinterpret_cast<char const*>(body.data()), body.size()});
  return key.string().ToString();
}

/// A TermExpr with the searched values evaluated
struct Expr {
  enum class Type { Term, And, Or, True, False, NeedsScan };
  Type type = Type::Term;
  /// Term: velocypack of the searched value
  std::string term;
  std::vector<Expr> children;
  std::vector<Expr> excluded;

  velocypack::Slice value() const {
    return velocypack::Slice(reinterpret_cast<uint8_t const*>(term.data()));
  }
  bool isConstant(bool v) const {
    return type == (v ? Type::True : Type::False);
  }
  /// all documents except the excluded ones
  bool isComplement() const {
    return type == Type::And && children.empty() && !excluded.empty();
  }
};

Expr evaluateValues(bitmap_index::TermExpr const& expr) {
  using Type = bitmap_index::TermExpr::Type;
  Expr result;
  switch (expr.type) {
    case Type::Term: {
      velocypack::Builder builder;
      expr.value->toVelocyPackValue(builder);
      result.term.assign(reinterpret_cast<char const*>(builder.data()),
                         builder.size());
      break;
    }
    case Type::True:
      result.type = Expr::Type::True;
      break;
    case Type::False:
      result.type = Expr::Type::False;
      break;
    case Type::And:
    case Type::Or:
      result.type = expr.type == Type::And ? Expr::Type::And : Expr::Type::Or;
      for (auto const& child : expr.children) {
        result.children.push_back(evaluateValues(child));
      }
      for (auto const& child : expr.excluded) {
        result.excluded.push_back(evaluateValues(child));
      }
      break;
  }
  return result;
}

/// Fold constants. The result is true, false, a complement (an And without
/// required children), NeedsScan if it cannot be answered from the bitmaps,
/// or an expression the bitmaps answer.
Expr fold(Expr const& expr) {
  using Type = Expr::Type;
  auto constant = [](bool value) {
    return Expr{.type = value ? Type::True : Type::False};
  };
  switch (expr.type) {
    case Type::Term:
    case Type::True:
    case Type::False:
    case Type::NeedsScan:
      return expr;
    case Type::And: {
      Expr result{.type = Type::And};
      for (auto const& child : expr.children) {
        Expr f = fold(child);
        if (f.isConstant(false) || f.type == Type::NeedsScan) {
          return f;
        }
        if (f.isComplement()) {
          for (auto& e : f.excluded) {
            result.excluded.push_back(std::move(e));
          }
        } else if (!f.isConstant(true)) {
          result.children.push_back(std::move(f));
        }
      }
      for (auto const& child : expr.excluded) {
        Expr f = fold(child);
        if (f.isConstant(true)) {
          return constant(false);
        }
        if (f.isComplement() || f.type == Type::NeedsScan) {
          return Expr{.type = Type::NeedsScan};
        }
        if (!f.isConstant(false)) {
          result.excluded.push_back(std::move(f));
        }
      }
      if (result.children.empty() && result.excluded.empty()) {
        return constant(true);
      }
      if (result.children.size() == 1 && result.excluded.empty()) {
        return std::move(result.children.front());
      }
      return result;
    }
    case Type::Or: {
      Expr result{.type = Type::Or};
      for (auto const& child : expr.children) {
        Expr f = fold(child);
        if (f.isConstant(true)) {
          return f;
        }
        if (f.isComplement() || f.type == Type::NeedsScan) {
          return Expr{.type = Type::NeedsScan};
        }
        if (!f.isConstant(false)) {
          result.children.push_back(std::move(f));
        }
      }
      if (result.children.empty()) {
        return constant(false);
      }
      if (result.children.size() == 1) {
        return std::move(result.children.front());
      }
      return result;
    }
  }
  TRI_ASSERT(false);
  return expr;
}

/// evaluate against the distinct elements of one document
bool evaluate(Expr const& expr, SliceSet const& elements) {
  using Type = Expr::Type;
  switch (expr.type) {
    case Type::Term:
      return elements.contains(expr.value());
    case Type::True:
      return true;
    case Type::False:
    case Type::NeedsScan:
      return false;
    case Type::And:
      return std::all_of(
                 expr.children.begin(), expr.children.end(),
                 [&](Expr const& c) { return evaluate(c, elements); }) &&
             std::none_of(
                 expr.excluded.begin(), expr.excluded.end(),
                 [&](Expr const& c) { return evaluate(c, elements); });
    case Type::Or:
      return std::any_of(
          expr.children.begin(), expr.children.end(),
          [&](Expr const& c) { return evaluate(c, elements); });
  }
  return false;
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
  TermNode(std::string countKey, rocksdb::Iterator& it,
           rocksdb::Comparator const& cmp)
      : _countKey(std::move(countKey)), _it(it), _cmp(cmp) {
    // the count key sorts directly before the element's shard keys
    _it.Seek(_countKey);
    if (_it.Valid() && suffixLength(_it.key()) == 0) {
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
    std::string key = _countKey;
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
  /// length of what follows the velocypack part of `key` if it belongs to
  /// this element, -1 otherwise. keys of equal values may differ in their
  /// bytes, so the comparator decides.
  std::int64_t suffixLength(rocksdb::Slice key) const {
    constexpr std::size_t objectIdSize = RocksDBKey::objectIdSize();
    if (key.size() <= objectIdSize) {
      return -1;
    }
    velocypack::Slice values(
        reinterpret_cast<uint8_t const*>(key.data()) + objectIdSize);
    std::size_t const end = objectIdSize + values.byteSize();
    if (end > key.size() ||
        _cmp.Compare(rocksdb::Slice(key.data(), end), _countKey) != 0) {
      return -1;
    }
    return static_cast<std::int64_t>(key.size() - end);
  }

  /// position on the shard key the iterator points to, if any
  void position() {
    _valid = false;
    if (!_it.Valid()) {
      rocksutils::checkIteratorStatus(_it);
      return;
    }
    rocksdb::Slice key = _it.key();
    if (suffixLength(key) != static_cast<std::int64_t>(bitmap_index::kShardBytes)) {
      return;
    }
    _shard = bitmap_index::readShard(key.data() + key.size() -
                                     bitmap_index::kShardBytes);
    _valid = true;
  }

  /// objectId | [prefix values..., element]
  std::string const _countKey;
  rocksdb::Iterator& _it;
  rocksdb::Comparator const& _cmp;
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
    if (_scanning) {
      return scan(callback, limit);
    }
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
    auto parts = bitmap_index::matchCondition(*_index, node, _reference,
                                              /*runtime*/ true);
    if (!parts.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported bitmap index condition");
    }

    _prefixValues.clear();
    _prefixValues.openArray();
    for (auto const* value : parts->prefixValues) {
      value->toVelocyPackValue(_prefixValues);
    }
    _prefixValues.close();
    _expr = evaluateValues(parts->expr);
    _folded = fold(_expr);
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

  std::unique_ptr<ShardNode> build(Expr const& expr) {
    switch (expr.type) {
      case Expr::Type::Term:
        return std::make_unique<TermNode>(
            makeKey(_index->objectId(), _prefixValues.slice(), expr.value()),
            nextIterator(), *_index->columnFamily()->GetComparator());
      case Expr::Type::And: {
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
      case Expr::Type::Or: {
        std::vector<std::unique_ptr<ShardNode>> children;
        for (auto const& child : expr.children) {
          children.push_back(build(child));
        }
        return std::make_unique<OrNode>(std::move(children));
      }
      case Expr::Type::True:
      case Expr::Type::False:
      case Expr::Type::NeedsScan:
        break;
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
    _scanning = false;
    _exhausted = _folded.isConstant(false);
    if (_exhausted) {
      return;
    }
    if (_folded.isConstant(true) || _folded.isComplement() ||
        _folded.type == Expr::Type::NeedsScan) {
      // the condition matches documents that are not in any bitmap, e.g.
      // for an empty array with ALL IN: check every document instead
      startScan();
      return;
    }
    _root = build(_folded);
  }

  void startScan() {
    _scanning = true;
    if (_scanIterator == nullptr) {
      auto* physical =
          static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
      _scanBounds.emplace(
          RocksDBKeyBounds::CollectionDocuments(physical->objectId()));
      _scanUpper = _scanBounds->end();
      RocksDBTransactionMethods* mthds =
          RocksDBTransactionState::toMethods(_trx, _collection->id());
      _scanIterator = mthds->NewIterator(
          _scanBounds->columnFamily(), [this](ReadOptions& options) {
            options.iterate_upper_bound = &_scanUpper;
            options.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
          });
    }
    _scanIterator->Seek(_scanBounds->start());
  }

  bool matches(velocypack::Slice doc) const {
    std::size_t i = 0;
    for (velocypack::Slice value :
         velocypack::ArrayIterator(_prefixValues.slice())) {
      if (!basics::VelocyPackHelper::equal(_index->prefixValue(doc, i++),
                                           value, true)) {
        return false;
      }
    }
    auto elements = _index->distinctElements(doc);
    return evaluate(_expr, SliceSet(elements.begin(), elements.end()));
  }

  bool scan(LocalDocumentIdCallback const& callback, uint64_t limit) {
    while (limit > 0 && _scanIterator->Valid()) {
      velocypack::Slice doc(
          reinterpret_cast<uint8_t const*>(_scanIterator->value().data()));
      LocalDocumentId id = RocksDBKey::documentId(_scanIterator->key());
      _scanIterator->Next();
      if (!matches(doc)) {
        continue;
      }
      callback(id);
      --limit;
    }
    if (!_scanIterator->Valid()) {
      rocksutils::checkIteratorStatus(*_scanIterator);
      return false;
    }
    return true;
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
  /// velocypack array of the searched prefix values
  velocypack::Builder _prefixValues;
  /// the condition with searched values evaluated, and with constants folded
  Expr _expr;
  Expr _folded;
  std::unique_ptr<ShardNode> _root;
  /// scan over all documents, for conditions the bitmaps cannot answer
  std::optional<RocksDBKeyBounds> _scanBounds;
  rocksdb::Slice _scanUpper;
  std::unique_ptr<rocksdb::Iterator> _scanIterator;
  bool _scanning = false;
  std::vector<std::unique_ptr<rocksdb::Iterator>> _iterators;
  std::size_t _iteratorsUsed = 0;
  std::vector<std::uint32_t> _buffer;
  std::size_t _pos = 0;
  std::uint64_t _bufferShard = 0;
  std::uint64_t _nextShard = 0;
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

velocypack::Slice RocksDBBitmapIndex::prefixValue(velocypack::Slice doc,
                                                  std::size_t i) const {
  TRI_ASSERT(i + 1 < _fields.size());
  velocypack::Slice value = extractField(doc, _fields[i]);
  return value.isNone() ? velocypack::Slice::nullSlice() : value;
}

std::vector<velocypack::Slice> RocksDBBitmapIndex::distinctElements(
    velocypack::Slice doc) const {
  std::vector<velocypack::Slice> result;
  velocypack::Slice value = extractField(doc, _fields.back());
  if (!value.isArray()) {
    return result;
  }
  SliceSet seen;
  for (velocypack::Slice element : velocypack::ArrayIterator(value)) {
    if (seen.insert(element).second) {
      result.push_back(element);
    }
  }
  return result;
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
  std::vector<velocypack::Slice> elements = distinctElements(doc);
  if (elements.empty()) {
    return {};
  }
  velocypack::Builder prefixValues;
  prefixValues.openArray();
  for (std::size_t i = 0; i + 1 < _fields.size(); ++i) {
    prefixValues.add(prefixValue(doc, i));
  }
  prefixValues.close();

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
  for (velocypack::Slice element : elements) {
    std::string countKey = makeKey(objectId(), prefixValues.slice(), element);
    key.constructFromBuffer(countKey);
    rocksdb::Status s = methods->MergeUntracked(_cf, key, countDelta);
    if (s.ok()) {
      bitmap_index::appendShard(countKey, bitmap_index::shardOf(id));
      key.constructFromBuffer(countKey);
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
