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
#include "RocksDBEngine/RocksDBBitmapIndexPendingWrites.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
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

  bool operator==(Expr const&) const = default;
};

/// evaluate the searched values into their stored form. a value that can
/// never be stored is in no document
template<typename Values>
Expr evaluateValues(bitmap_index::TermExpr const& expr) {
  using Type = bitmap_index::TermExpr::Type;
  Expr result;
  switch (expr.type) {
    case Type::Term: {
      velocypack::Builder value;
      expr.value->toVelocyPackValue(value);
      velocypack::Builder stored;
      if (!Values::add(stored, value.slice())) {
        result.type = Expr::Type::False;
        break;
      }
      result.term.assign(reinterpret_cast<char const*>(stored.data()),
                         stored.size());
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
        result.children.push_back(evaluateValues<Values>(child));
      }
      for (auto const& child : expr.excluded) {
        result.excluded.push_back(evaluateValues<Values>(child));
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

/// A Roaring bitmap that is either a read-only view of a RocksDB value (no
/// copy of its containers) or owned. Views stay valid until the RocksDB
/// iterator holding the value moves.
class BitmapRef {
 public:
  using Bitmap = roaring::api::roaring_bitmap_t;

  static BitmapRef view(rocksdb::Slice value) {
    std::string_view v{value.data(), value.size()};
    if (bitmap_index::tagOf(v) == bitmap_index::ValueTag::Bitmap) {
      Bitmap* frozen =
          roaring::api::roaring_bitmap_portable_deserialize_frozen(v.data() + 1);
      if (frozen == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid bitmap index value");
      }
      return BitmapRef(frozen, /*isView*/ true);
    }
    // an unmerged delta, decoded into its own containers
    roaring::Roaring decoded = bitmap_index::decodeBitmap(v);
    return BitmapRef(roaring::api::roaring_bitmap_copy(&decoded.roaring),
                     /*isView*/ false);
  }

  Bitmap const* get() const { return _bitmap.get(); }
  bool empty() const { return roaring::api::roaring_bitmap_is_empty(get()); }

  void intersect(BitmapRef const& other) {
    combine(other, roaring::api::roaring_bitmap_and,
            roaring::api::roaring_bitmap_and_inplace);
  }
  void unite(BitmapRef const& other) {
    combine(other, roaring::api::roaring_bitmap_or,
            roaring::api::roaring_bitmap_or_inplace);
  }
  void subtract(BitmapRef const& other) {
    combine(other, roaring::api::roaring_bitmap_andnot,
            roaring::api::roaring_bitmap_andnot_inplace);
  }

 private:
  BitmapRef(Bitmap* bitmap, bool isView) : _bitmap(bitmap), _isView(isView) {}

  /// a view cannot be modified, so the result becomes a new owned bitmap
  template<typename Make, typename InPlace>
  void combine(BitmapRef const& other, Make make, InPlace inPlace) {
    if (_isView) {
      _bitmap.reset(make(get(), other.get()));
      _isView = false;
    } else {
      inPlace(_bitmap.get(), other.get());
    }
  }

  struct Free {
    void operator()(Bitmap* b) const { roaring::api::roaring_bitmap_free(b); }
  };
  std::unique_ptr<Bitmap, Free> _bitmap;
  bool _isView = false;
};

/// Iterates the shards that may hold results of one part of a condition, in
/// ascending order. seek() targets never decrease. The tree is built once per
/// condition; restart() binds it to (new) prefix values, e.g. per vertex.
class ShardNode {
 public:
  virtual ~ShardNode() = default;
  virtual void restart(velocypack::Slice prefixValues) = 0;
  /// position on the first shard >= target that may hold results; false if
  /// there is none
  virtual bool seek(std::uint64_t target) = 0;
  virtual std::uint64_t shard() const = 0;
  /// matching offsets in the current shard
  virtual BitmapRef bitmap() = 0;
  /// estimated number of matching documents
  virtual std::int64_t cost() const = 0;
};

class TermNode final : public ShardNode {
 public:
  TermNode(std::uint64_t objectId, velocypack::Slice element,
           rocksdb::Iterator& it, rocksdb::Comparator const& cmp)
      : _objectId(objectId),
        _element(reinterpret_cast<char const*>(element.start()),
                 element.byteSize()),
        _it(it),
        _cmp(cmp) {}

  void restart(velocypack::Slice prefixValues) override {
    _countKey = makeKey(
        _objectId, prefixValues,
        velocypack::Slice(reinterpret_cast<uint8_t const*>(_element.data())));
    _count = 0;
    _valid = false;
    // the count key sorts directly before the element's shard keys
    _it.Seek(_countKey);
    if (_it.Valid() && suffixLength(_it.key()) == 0) {
      rocksdb::Slice value = _it.value();
      _count = bitmap_index::decodeCount({value.data(), value.size()});
      _it.Next();
    }
    // without documents for this element there is nothing to find
    _exhausted = _count <= 0;
    if (!_exhausted) {
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
    _shardKey.assign(_countKey);
    bitmap_index::appendShard(_shardKey, target);
    _it.Seek(_shardKey);
    position();
    _exhausted = !_valid;
    return _valid;
  }

  std::uint64_t shard() const override { return _shard; }

  BitmapRef bitmap() override {
    TRI_ASSERT(_valid);
    return BitmapRef::view(_it.value());
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
    if (suffixLength(key) !=
        static_cast<std::int64_t>(bitmap_index::kShardBytes)) {
      return;
    }
    _shard = bitmap_index::readShard(key.data() + key.size() -
                                     bitmap_index::kShardBytes);
    _valid = true;
  }

  std::uint64_t const _objectId;
  /// stored form of the searched element
  std::string const _element;
  rocksdb::Iterator& _it;
  rocksdb::Comparator const& _cmp;
  /// objectId | [prefix values..., element]
  std::string _countKey;
  std::string _shardKey;
  std::int64_t _count = 0;
  std::uint64_t _shard = 0;
  bool _valid = false;
  bool _exhausted = true;
};

/// intersection of the required children, minus the excluded ones
class AndNode final : public ShardNode {
 public:
  AndNode(std::vector<std::unique_ptr<ShardNode>> required,
          std::vector<std::unique_ptr<ShardNode>> excluded)
      : _required(std::move(required)), _excluded(std::move(excluded)) {
    TRI_ASSERT(!_required.empty());
  }

  void restart(velocypack::Slice prefixValues) override {
    for (auto& child : _required) {
      child->restart(prefixValues);
    }
    for (auto& child : _excluded) {
      child->restart(prefixValues);
    }
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

  BitmapRef bitmap() override {
    BitmapRef result = _required[0]->bitmap();
    for (std::size_t i = 1; i < _required.size() && !result.empty(); ++i) {
      result.intersect(_required[i]->bitmap());
    }
    for (auto& excluded : _excluded) {
      if (result.empty()) {
        break;
      }
      if (excluded->seek(_shard) && excluded->shard() == _shard) {
        result.subtract(excluded->bitmap());
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
      : _children(std::move(children)) {}

  void restart(velocypack::Slice prefixValues) override {
    _cost = 0;
    for (auto& child : _children) {
      child->restart(prefixValues);
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

  BitmapRef bitmap() override {
    std::optional<BitmapRef> result;
    for (auto& child : _children) {
      if (child->seek(_shard) && child->shard() == _shard) {
        if (result.has_value()) {
          result->unite(child->bitmap());
        } else {
          result.emplace(child->bitmap());
        }
      }
    }
    TRI_ASSERT(result.has_value());
    return std::move(*result);
  }

  std::int64_t cost() const override { return _cost; }

 private:
  std::vector<std::unique_ptr<ShardNode>> _children;
  std::int64_t _cost = 0;
  std::uint64_t _shard = 0;
};

template<typename Values>
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

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    if (_index->coveredFields().empty()) {
      // nothing is covered; late materialization only needs the ids
      SliceCoveringData covering(velocypack::Slice::emptyArraySlice());
      return nextImpl(
          [&](LocalDocumentId id) { return callback(id, covering); }, limit);
    }
    // [vertex, other vertex] per edge: the vertex is the searched prefix
    // value, the other vertex comes from the edge index
    velocypack::Slice vertex = _prefixValues.slice().at(0);
    velocypack::Builder covering;
    return nextImpl(
        [&](LocalDocumentId id) {
          covering.clear();
          covering.openArray();
          covering.add(vertex);
          addOtherVertex(covering, vertex, id);
          covering.close();
          SliceCoveringData data(covering.slice());
          return callback(id, data);
        },
        limit);
  }

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
    if (canReadOwnWrites() == ReadOwnWrites::yes) {
      // own writes are only visible to RocksDB once they are flushed
      RocksDBTransactionMethods* mthds =
          RocksDBTransactionState::toMethods(_trx, _collection->id());
      if (auto* pending = mthds->bitmapIndexWrites();
          pending != nullptr && !pending->empty()) {
        if (rocksdb::Status s = pending->flush(*mthds); !s.ok()) {
          THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(s));
        }
      }
    }
    auto parts = bitmap_index::matchCondition(*_index, node, _reference,
                                              /*runtime*/ true);
    if (!parts.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported bitmap index condition");
    }

    _usable = true;
    _prefixValues.clear();
    _prefixValues.openArray();
    for (auto const* node : parts->prefixValues) {
      velocypack::Builder value;
      node->toVelocyPackValue(value);
      // a value that can never be stored is in no document
      _usable = _usable && Values::add(_prefixValues, value.slice());
    }
    _prefixValues.close();
    Expr expr = evaluateValues<Values>(parts->expr);
    if (_root == nullptr || expr != _expr) {
      // usually only the prefix values change, e.g. per traversal vertex;
      // then the tree is kept and only bound to them again
      _expr = std::move(expr);
      _folded = fold(_expr);
      _root.reset();
    }
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
            _index->objectId(), expr.value(), nextIterator(),
            *_index->columnFamily()->GetComparator());
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
    _scanning = false;
    _exhausted = !_usable || _folded.isConstant(false);
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
    if (_root == nullptr) {
      _iteratorsUsed = 0;
      _root = build(_folded);
    }
    _root->restart(_prefixValues.slice());
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
      BitmapRef result = _root->bitmap();
      _nextShard = shard + 1;
      if (!result.empty()) {
        _buffer.resize(roaring::api::roaring_bitmap_get_cardinality(result.get()));
        roaring::api::roaring_bitmap_to_uint32_array(result.get(),
                                                     _buffer.data());
        _bufferShard = shard;
        _pos = 0;
        return true;
      }
    }
    return false;
  }

  /// Merge join with the edge index: matching edge ids arrive in ascending
  /// order, and so do the edge index entries of one vertex. Close targets are
  /// reached with a few Next() calls, far ones with a Seek().
  void addOtherVertex(velocypack::Builder& out, velocypack::Slice vertex,
                      LocalDocumentId id) {
    if (_edgeIndex == nullptr) {
      std::string const& attribute = _index->fields()[0][0].name;
      for (auto const& idx : _collection->getPhysical()->getReadyIndexes()) {
        if (idx->type() == IndexType::Edge &&
            idx->fields()[0][0].name == attribute) {
          _edgeIndex = static_cast<RocksDBIndex const*>(idx.get());
          break;
        }
      }
      TRI_ASSERT(_edgeIndex != nullptr);
      RocksDBTransactionMethods* mthds =
          RocksDBTransactionState::toMethods(_trx, _collection->id());
      _edgeIterator = mthds->NewIterator(
          _edgeIndex->columnFamily(), [this](ReadOptions& options) {
            options.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes;
          });
    }

    std::string_view v = vertex.stringView();
    auto atTarget = [&]() -> std::optional<int> {
      // compares the current entry with the target: <0, 0 or >0, or none if
      // the iterator left the vertex
      if (!_edgeIterator->Valid()) {
        return std::nullopt;
      }
      rocksdb::Slice key = _edgeIterator->key();
      if (RocksDBKey::vertexId(key) != v) {
        return std::nullopt;
      }
      auto current = RocksDBKey::edgeDocumentId(key).id();
      return current < id.id() ? -1 : (current == id.id() ? 0 : 1);
    };

    std::optional<int> cmp;
    if (_edgeVertex == v) {
      cmp = atTarget();
      for (int steps = 0; steps < 8 && cmp == -1; ++steps) {
        _edgeIterator->Next();
        cmp = atTarget();
      }
    }
    if (cmp != 0) {
      _edgeVertex.assign(v);
      RocksDBKey key;
      key.constructEdgeIndexValue(_edgeIndex->objectId(), v, id);
      _edgeIterator->Seek(key.string());
      cmp = atTarget();
    }
    if (cmp == 0) {
      out.add(velocypack::Value(RocksDBValue::vertexId(_edgeIterator->value())));
      return;
    }

    // not expected, both indexes are read from the same snapshot: take the
    // other vertex from the edge document
    rocksutils::checkIteratorStatus(*_edgeIterator);
    bool const fromIndex =
        _index->fields()[0][0].name == StaticStrings::FromString;
    std::ignore = _collection->getPhysical()->lookup(
        _trx, id,
        [&](LocalDocumentId, aql::DocumentData&&, velocypack::Slice doc) {
          out.add(fromIndex ? transaction::helpers::extractToFromDocument(doc)
                            : transaction::helpers::extractFromFromDocument(doc));
          return true;
        },
        {.readOwnWrites = canReadOwnWrites() == ReadOwnWrites::yes});
  }

  RocksDBBitmapIndex const* _index;
  aql::Variable const* _reference;
  /// edge index on the prefix attribute, for covering the other vertex
  RocksDBIndex const* _edgeIndex = nullptr;
  std::unique_ptr<rocksdb::Iterator> _edgeIterator;
  std::string _edgeVertex;
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
  bool _usable = false;
  bool _exhausted = true;
};

}  // namespace

bool bitmap_index::ScalarValues::add(velocypack::Builder& out,
                                     velocypack::Slice value) {
  if (value.isNumber()) {
    double d = value.getNumber<double>();
    // -0.0 and 0.0 compare equal
    out.add(velocypack::Value(d == 0.0 ? 0.0 : d));
  } else if (value.isString()) {
    out.add(velocypack::Value(value.stringView()));
  } else if (value.isBoolean() || value.isNull()) {
    out.add(value);
  } else {
    return false;
  }
  return true;
}

std::shared_ptr<RocksDBBitmapIndex> RocksDBBitmapIndex::create(
    IndexId iid, LogicalCollection& collection, velocypack::Slice info) {
  if (info.get(kValuesAttribute).isEqualString(
          bitmap_index::ScalarValues::kName)) {
    return std::make_shared<
        RocksDBBitmapIndexImpl<bitmap_index::ScalarValues>>(iid, collection,
                                                           info);
  }
  return std::make_shared<RocksDBBitmapIndexImpl<bitmap_index::AnyValues>>(
      iid, collection, info);
}

RocksDBKeyBounds RocksDBBitmapIndex::boundsFor(velocypack::Slice definition,
                                               std::uint64_t objectId) {
  return RocksDBKeyBounds::BitmapIndex(
      objectId, definition.get(kValuesAttribute)
                        .isEqualString(bitmap_index::ScalarValues::kName)
                    ? bitmap_index::ScalarValues::kEntryType
                    : bitmap_index::AnyValues::kEntryType);
}

RocksDBBitmapIndex::RocksDBBitmapIndex(IndexId iid,
                                       LogicalCollection& collection,
                                       velocypack::Slice info,
                                       rocksdb::ColumnFamilyHandle* cf)
    : RocksDBIndex(iid, collection, info, cf,
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   collection.vocbase().engine<RocksDBEngine>()),
      _coveredFields(coveredFieldsFor(collection, _fields)) {
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
  builder.add(kValuesAttribute, velocypack::Value(valuesName()));
  builder.close();
}

std::vector<std::vector<basics::AttributeName>>
RocksDBBitmapIndex::coveredFieldsFor(
    LogicalCollection const& collection,
    std::vector<std::vector<basics::AttributeName>> const& fields) {
  if (collection.type() != TRI_COL_TYPE_EDGE || fields.size() != 2 ||
      fields[0].size() != 1) {
    return {};
  }
  std::string const& prefix = fields[0][0].name;
  if (prefix != StaticStrings::FromString &&
      prefix != StaticStrings::ToString) {
    return {};
  }
  std::string const& other = prefix == StaticStrings::FromString
                                 ? StaticStrings::ToString
                                 : StaticStrings::FromString;
  return {{basics::AttributeName(prefix, false)},
          {basics::AttributeName(other, false)}};
}

bool RocksDBBitmapIndex::matchesDefinition(velocypack::Slice const& info) const {
  if (!RocksDBIndex::matchesDefinition(info)) {
    return false;
  }
  // how values are stored is part of the identity; "any" if not given
  velocypack::Slice values = info.get(kValuesAttribute);
  return (values.isString() ? values.stringView()
                            : bitmap_index::AnyValues::kName) == valuesName();
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

template<typename Values>
RocksDBBitmapIndexImpl<Values>::RocksDBBitmapIndexImpl(
    IndexId iid, LogicalCollection& collection, velocypack::Slice info)
    : RocksDBBitmapIndex(iid, collection, info,
                         RocksDBColumnFamilyManager::get(Values::kFamily)) {}

template<typename Values>
std::unique_ptr<IndexIterator>
RocksDBBitmapIndexImpl<Values>::iteratorForCondition(
    ResourceMonitor& /*monitor*/, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& /*opts*/, ReadOwnWrites readOwnWrites,
    int /*mutableConditionIdx*/) {
  return std::make_unique<RocksDBBitmapIndexIterator<Values>>(
      &_collection, trx, this, node, reference, readOwnWrites);
}

template<typename Values>
Result RocksDBBitmapIndexImpl<Values>::insert(
    transaction::Methods& /*trx*/, RocksDBMethods* methods,
    LocalDocumentId documentId, velocypack::Slice doc,
    OperationOptions const& /*options*/, bool /*performChecks*/) {
  return write(methods, documentId, doc, /*isInsert*/ true);
}

template<typename Values>
Result RocksDBBitmapIndexImpl<Values>::remove(
    transaction::Methods& /*trx*/, RocksDBMethods* methods,
    LocalDocumentId documentId, velocypack::Slice doc,
    OperationOptions const& /*options*/) {
  return write(methods, documentId, doc, /*isInsert*/ false);
}

template<typename Values>
Result RocksDBBitmapIndexImpl<Values>::write(RocksDBMethods* methods,
                                             LocalDocumentId documentId,
                                             velocypack::Slice doc,
                                             bool isInsert) {
  auto rejected = [&](velocypack::Slice value) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("bitmap index '", name(), "' with values '",
                               Values::kName, "' cannot index ",
                               value.typeName(), " values"));
  };

  std::vector<velocypack::Slice> elements = distinctElements(doc);
  if (elements.empty()) {
    return {};
  }
  velocypack::Builder prefixValues;
  prefixValues.openArray();
  for (std::size_t i = 0; i + 1 < _fields.size(); ++i) {
    if (velocypack::Slice value = prefixValue(doc, i);
        !Values::add(prefixValues, value)) {
      return rejected(value);
    }
  }
  prefixValues.close();

  std::uint64_t const id = documentId.id();
  std::uint32_t const offset = bitmap_index::offsetOf(id);
  bitmap_index::PendingWrites* pending = methods->bitmapIndexWrites();
  std::string delta;
  std::string countDelta;
  if (pending == nullptr) {
    roaring::Roaring single;
    single.add(offset);
    roaring::Roaring none;
    delta = isInsert ? bitmap_index::encodeDelta(single, none)
                     : bitmap_index::encodeDelta(none, single);
    countDelta = bitmap_index::encodeCount(bitmap_index::ValueTag::CountDelta,
                                           isInsert ? 1 : -1);
  }

  Result res;
  RocksDBKey key;
  velocypack::Builder stored;
  std::string bitmapKey;
  for (velocypack::Slice element : elements) {
    stored.clear();
    if (!Values::add(stored, element)) {
      return rejected(element);
    }
    std::string const countKey =
        makeKey(objectId(), prefixValues.slice(), stored.slice());
    bitmapKey = countKey;
    bitmap_index::appendShard(bitmapKey, bitmap_index::shardOf(id));
    if (pending != nullptr) {
      pending->add(_cf, countKey, bitmapKey, offset, isInsert);
      continue;
    }
    key.constructFromBuffer(countKey);
    rocksdb::Status s = methods->MergeUntracked(_cf, key, countDelta);
    if (s.ok()) {
      key.constructFromBuffer(bitmapKey);
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

namespace arangodb {
template class RocksDBBitmapIndexImpl<bitmap_index::AnyValues>;
template class RocksDBBitmapIndexImpl<bitmap_index::ScalarValues>;
}  // namespace arangodb
