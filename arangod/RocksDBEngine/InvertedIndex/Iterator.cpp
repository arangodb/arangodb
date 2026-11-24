////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/InvertedIndex/Iterator.h"

#include "Basics/Exceptions.h"
#include "Basics/overload.h"
#include "Basics/voc-errors.h"
#include "RocksDBEngine/InvertedIndex/Matcher.h"
#include "RocksDBEngine/InvertedIndex/RocksDBInvertedIndex.h"
#include "RocksDBEngine/InvertedIndex/Expr.h"
#include "RocksDBEngine/InvertedIndex/Format.h"
#include "RocksDBEngine/InvertedIndex/PendingWrites.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/iterator.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <functional>
#include <limits>
#include <optional>
#include <variant>

#include "roaring.hh"

namespace arangodb {
namespace {

using inverted_index::Expr;

/// most ids read from a result bitmap at a time
constexpr std::uint64_t kReadBatch = 4096;
/// ids per Roaring container, the unit of a cheap first part of a result
constexpr std::uint32_t kChunkSize = 1 << 16;

std::string_view view(rocksdb::Slice s) { return {s.data(), s.size()}; }

/// A Roaring bitmap that is either owned or borrowed. A borrowed bitmap
/// belongs to a TermNode and stays valid until that node moves to another
/// shard.
class BitmapRef {
 public:
  using Bitmap = roaring::api::roaring_bitmap_t;

  static BitmapRef owned(Bitmap* bitmap) { return BitmapRef(bitmap, bitmap); }
  static BitmapRef borrowed(Bitmap const* bitmap) {
    return BitmapRef(bitmap, nullptr);
  }

  Bitmap const* get() const { return _bitmap; }
  bool empty() const { return roaring::api::roaring_bitmap_is_empty(get()); }

  void intersect(BitmapRef const& other) {
    combine(other, roaring::api::roaring_bitmap_and,
            roaring::api::roaring_bitmap_and_inplace);
  }
  void subtract(BitmapRef const& other) {
    combine(other, roaring::api::roaring_bitmap_andnot,
            roaring::api::roaring_bitmap_andnot_inplace);
  }

 private:
  BitmapRef(Bitmap const* bitmap, Bitmap* owned)
      : _bitmap(bitmap), _owned(owned) {}

  /// a borrowed bitmap cannot be modified, so the result becomes a new owned
  /// bitmap
  template<typename Make, typename InPlace>
  void combine(BitmapRef const& other, Make make, InPlace inPlace) {
    if (_owned == nullptr) {
      _owned.reset(make(get(), other.get()));
      _bitmap = _owned.get();
    } else {
      inPlace(_owned.get(), other.get());
    }
  }

  struct Free {
    void operator()(Bitmap* b) const { roaring::api::roaring_bitmap_free(b); }
  };
  Bitmap const* _bitmap;
  std::unique_ptr<Bitmap, Free> _owned;
};

/// The bitmap of a RocksDB value. A merged value is deserialized without a
/// copy of its containers, so the result stays valid only until the RocksDB
/// iterator holding the value moves.
BitmapRef bitmapOf(rocksdb::Slice value) {
  std::string_view v{value.data(), value.size()};
  if (inverted_index::tagOf(v) == inverted_index::ValueTag::Bitmap) {
    auto* frozen =
        roaring::api::roaring_bitmap_portable_deserialize_frozen(v.data() + 1);
    if (frozen == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid RocksDB inverted index value");
    }
    return BitmapRef::owned(frozen);
  }
  // an unmerged delta, decoded into its own containers
  roaring::Roaring decoded = inverted_index::decodeBitmap(v);
  return BitmapRef::owned(roaring::api::roaring_bitmap_copy(&decoded.roaring));
}

/// Lazy unions compute the cardinality of each container once, in the repair
/// at the end, instead of after every step. They run without bitset
/// conversion, because converting sparse array containers to bitsets and back
/// in the repair costs more than it saves. With many sparse inputs the lazy
/// union is slower than plain unions, measured with 512 rare terms, so plain
/// unions are used above kLazyUnionInputs.
BitmapRef unite(std::vector<BitmapRef> const& parts) {
  constexpr std::size_t kLazyUnionInputs = 32;
  constexpr bool kBitsetConversion = false;
  TRI_ASSERT(parts.size() >= 2);
  if (parts.size() > kLazyUnionInputs) {
    auto* result =
        roaring::api::roaring_bitmap_or(parts[0].get(), parts[1].get());
    for (std::size_t i = 2; i < parts.size(); ++i) {
      roaring::api::roaring_bitmap_or_inplace(result, parts[i].get());
    }
    return BitmapRef::owned(result);
  }
  auto* result = roaring::api::roaring_bitmap_lazy_or(
      parts[0].get(), parts[1].get(), kBitsetConversion);
  for (std::size_t i = 2; i < parts.size(); ++i) {
    roaring::api::roaring_bitmap_lazy_or_inplace(result, parts[i].get(),
                                                 kBitsetConversion);
  }
  roaring::api::roaring_bitmap_repair_after_lazy(result);
  return BitmapRef::owned(result);
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
  /// matching offsets in the current shard; only those in `range` if it is
  /// not null
  virtual BitmapRef bitmap(BitmapRef::Bitmap const* range) = 0;
  /// bitmaps whose union is bitmap(range); a reader can merge them instead of
  /// building the union
  virtual std::vector<BitmapRef> parts(BitmapRef::Bitmap const* range) {
    std::vector<BitmapRef> result;
    result.push_back(bitmap(range));
    return result;
  }
  /// no matching offset in the current shard is smaller
  virtual std::uint32_t minimum() = 0;
  /// estimated number of matching documents
  virtual std::int64_t cost() const = 0;
};

class TermNode final : public ShardNode {
 public:
  TermNode(rocksdb::Iterator& it, std::string key)
      : _it(it), _key(std::move(key)), _termKeySize(_key.size()) {
    _it.Seek(termKey());
    if (_it.Valid() && view(_it.key()) == termKey()) {
      _count = inverted_index::decodeCount(view(_it.value()));
      _it.Next();
    }
    // without documents for this term there is nothing to find
    if (_count > 0) {
      _shard = currentShard();
    }
    // from now on _key is a bitmap key, its shard is set before each seek
    _key.resize(_termKeySize + inverted_index::kShardBytes);
  }

  bool seek(std::uint64_t target) override {
    if (!_shard.has_value() || *_shard >= target) {
      return _shard.has_value();
    }
    _current.reset();
    // The target is usually the next shard of the term, which is the next
    // key. A step is much cheaper than a seek in RocksDB.
    _it.Next();
    _shard = currentShard();
    if (_shard.has_value() && *_shard < target) {
      inverted_index::setShardOfBitmapKey(_key, target);
      _it.Seek(_key);
      _shard = currentShard();
    }
    return _shard.has_value();
  }

  std::uint64_t shard() const override { return *_shard; }

  BitmapRef bitmap(BitmapRef::Bitmap const* range) override {
    BitmapRef::Bitmap const* all = current().get();
    if (range == nullptr) {
      return BitmapRef::borrowed(all);
    }
    return BitmapRef::owned(roaring::api::roaring_bitmap_and(all, range));
  }

  std::uint32_t minimum() override {
    return roaring::api::roaring_bitmap_minimum(current().get());
  }

  std::int64_t cost() const override { return _count; }

 private:
  std::string_view termKey() const {
    return std::string_view(_key).substr(0, _termKeySize);
  }

  std::optional<std::uint64_t> currentShard() {
    if (!_it.Valid()) {
      rocksutils::checkIteratorStatus(_it);
      return std::nullopt;
    }
    return inverted_index::shardOfBitmapKey(view(_it.key()), termKey());
  }

  /// deserializing is linear in the number of containers, so it is done once
  /// per shard
  BitmapRef const& current() {
    if (!_current.has_value()) {
      _current = bitmapOf(_it.value());
    }
    return *_current;
  }

  rocksdb::Iterator& _it;
  /// the term key, followed by a shard after construction
  std::string _key;
  std::size_t const _termKeySize;
  std::int64_t _count = 0;
  std::optional<std::uint64_t> _shard;
  /// the bitmap of the current shard; borrowed by the results of bitmap()
  std::optional<BitmapRef> _current;
};

/// intersection of the required children, minus the excluded ones
class AndNode final : public ShardNode {
 public:
  AndNode(std::vector<std::unique_ptr<ShardNode>> required,
          std::vector<std::unique_ptr<ShardNode>> excluded)
      : _required(std::move(required)), _excluded(std::move(excluded)) {
    TRI_ASSERT(!_required.empty());
    // the rarest child drives the leapfrog
    std::ranges::sort(_required, std::less{},
                      [](auto const& child) { return child->cost(); });
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

  BitmapRef bitmap(BitmapRef::Bitmap const* range) override {
    // restricting the first child restricts the intersection
    BitmapRef result = _required[0]->bitmap(range);
    for (std::size_t i = 1; i < _required.size() && !result.empty(); ++i) {
      result.intersect(_required[i]->bitmap(nullptr));
    }
    for (auto& excluded : _excluded) {
      if (result.empty()) {
        break;
      }
      if (excluded->seek(_shard) && excluded->shard() == _shard) {
        result.subtract(excluded->bitmap(nullptr));
      }
    }
    return result;
  }

  std::uint32_t minimum() override {
    std::uint32_t result = 0;
    for (auto& child : _required) {
      result = std::max(result, child->minimum());
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

  BitmapRef bitmap(BitmapRef::Bitmap const* range) override {
    std::vector<BitmapRef> all = parts(range);
    if (all.size() == 1) {
      return std::move(all.front());
    }
    return unite(all);
  }

  std::vector<BitmapRef> parts(BitmapRef::Bitmap const* range) override {
    std::vector<BitmapRef> result;
    for (auto& child : _children) {
      if (isOnShard(*child)) {
        std::ranges::move(child->parts(range), std::back_inserter(result));
      }
    }
    TRI_ASSERT(!result.empty());
    return result;
  }

  std::uint32_t minimum() override {
    std::uint32_t result = std::numeric_limits<std::uint32_t>::max();
    for (auto& child : _children) {
      if (isOnShard(*child)) {
        result = std::min(result, child->minimum());
      }
    }
    return result;
  }

  std::int64_t cost() const override { return _cost; }

 private:
  bool isOnShard(ShardNode& child) const {
    return child.seek(_shard) && child.shard() == _shard;
  }

  std::vector<std::unique_ptr<ShardNode>> _children;
  std::int64_t _cost = 0;
  std::uint64_t _shard = 0;
};

/// RocksDB iterators are kept across rearms, only the keys change
class IteratorPool {
 public:
  using Factory = std::function<std::unique_ptr<rocksdb::Iterator>()>;

  explicit IteratorPool(Factory factory) : _factory(std::move(factory)) {}

  rocksdb::Iterator& next() {
    if (_used == _iterators.size()) {
      _iterators.push_back(_factory());
    }
    return *_iterators[_used++];
  }

  void reset() noexcept { _used = 0; }

 private:
  Factory _factory;
  std::vector<std::unique_ptr<rocksdb::Iterator>> _iterators;
  std::size_t _used = 0;
};

/// `folded` must be answered by the bitmaps alone, see needsScan
std::unique_ptr<ShardNode> buildTree(Expr const& folded, std::uint64_t objectId,
                                     IteratorPool& iterators) {
  auto buildAll = [&](std::vector<Expr> const& exprs) {
    std::vector<std::unique_ptr<ShardNode>> nodes;
    nodes.reserve(exprs.size());
    for (auto const& e : exprs) {
      nodes.push_back(buildTree(e, objectId, iterators));
    }
    return nodes;
  };
  switch (folded.type) {
    case Expr::Type::Term:
      return std::make_unique<TermNode>(
          iterators.next(), inverted_index::termKey(objectId, folded.term));
    case Expr::Type::And:
      return std::make_unique<AndNode>(buildAll(folded.children),
                                       buildAll(folded.excluded));
    case Expr::Type::Or:
      return std::make_unique<OrNode>(buildAll(folded.children));
    case Expr::Type::True:
    case Expr::Type::False:
    case Expr::Type::NeedsScan:
      break;
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "rocksdb-inverted index condition needs a scan");
}

/// The offsets of one shard in ascending order, read from one bitmap or
/// merged from the bitmaps of a union. Merging while reading avoids building
/// the union, which copies every container of its inputs.
class ShardReader {
 public:
  /// Unions with more inputs than this are built before reading, because
  /// each merge step compares the current offsets of all inputs.
  static constexpr std::size_t kMaxMergedInputs = 16;

  /// reads `bitmaps` from offset `from` on
  ShardReader(std::vector<BitmapRef> bitmaps, std::uint32_t from) {
    TRI_ASSERT(!bitmaps.empty());
    TRI_ASSERT(bitmaps.size() <= kMaxMergedInputs);
    _inputs.reserve(bitmaps.size());
    for (auto& bitmap : bitmaps) {
      Input& input = _inputs.emplace_back(std::move(bitmap));
      roaring::api::roaring_iterator_init(input.bitmap.get(), &input.it);
      if (from > 0) {
        roaring::api::roaring_uint32_iterator_move_equalorlarger(&input.it,
                                                                 from);
      }
    }
    if (isSingle()) {
      auto const* bitmap = _inputs[0].bitmap.get();
      _unread = roaring::api::roaring_bitmap_get_cardinality(bitmap);
      if (from > 0) {
        _unread -= roaring::api::roaring_bitmap_rank(bitmap, from - 1);
      }
    }
  }

  // the iterators point to the bitmaps of the inputs
  ShardReader(ShardReader const&) = delete;
  ShardReader& operator=(ShardReader const&) = delete;

  bool exhausted() const {
    if (isSingle()) {
      return _unread == 0;
    }
    return std::ranges::none_of(
        _inputs, [](Input const& input) { return input.it.has_value; });
  }

  /// reads at most out.size() offsets into out, returns how many
  std::size_t read(std::span<std::uint32_t> out) {
    if (isSingle()) {
      std::uint32_t const n = roaring::api::roaring_uint32_iterator_read(
          &_inputs[0].it, out.data(),
          static_cast<std::uint32_t>(
              std::min<std::uint64_t>(out.size(), _unread)));
      _unread -= n;
      return n;
    }
    std::size_t n = 0;
    while (n < out.size() && !exhausted()) {
      out[n++] = takeSmallest();
    }
    return n;
  }

  /// skips at most `count` offsets, returns how many
  std::uint64_t skip(std::uint64_t count) {
    if (isSingle()) {
      return skipSingle(count);
    }
    std::uint64_t skipped = 0;
    for (; skipped < count && !exhausted(); ++skipped) {
      takeSmallest();
    }
    return skipped;
  }

 private:
  struct Input {
    explicit Input(BitmapRef b) : bitmap(std::move(b)) {}
    BitmapRef bitmap;
    roaring::api::roaring_uint32_iterator_t it{};
  };

  bool isSingle() const { return _inputs.size() == 1; }

  /// skips by cardinality, without visiting single offsets
  std::uint64_t skipSingle(std::uint64_t count) {
    std::uint64_t const n = std::min(count, _unread);
    if (n > 0 && n < _unread) {
      auto const* bitmap = _inputs[0].bitmap.get();
      std::uint64_t const read =
          roaring::api::roaring_bitmap_get_cardinality(bitmap) - _unread;
      std::uint32_t next = 0;
      bool const found = roaring::api::roaring_bitmap_select(
          bitmap, static_cast<std::uint32_t>(read + n), &next);
      TRI_ASSERT(found);
      roaring::api::roaring_uint32_iterator_move_equalorlarger(&_inputs[0].it,
                                                               next);
    }
    _unread -= n;
    return n;
  }

  /// the smallest current offset of all inputs; every input at that offset
  /// moves on, so that an offset in several inputs is taken once
  std::uint32_t takeSmallest() {
    std::uint32_t smallest = std::numeric_limits<std::uint32_t>::max();
    for (auto const& input : _inputs) {
      if (input.it.has_value) {
        smallest = std::min(smallest, input.it.current_value);
      }
    }
    for (auto& input : _inputs) {
      if (input.it.has_value && input.it.current_value == smallest) {
        roaring::api::roaring_uint32_iterator_advance(&input.it);
      }
    }
    return smallest;
  }

  std::vector<Input> _inputs;
  /// offsets left to read, only tracked for a single input
  std::uint64_t _unread = 0;
};

/// Reads the ids matched by a tree of ShardNodes in ascending order, one
/// shard at a time.
class BitmapCursor {
 public:
  BitmapCursor(std::unique_ptr<ShardNode> root, inverted_index::IdLayout layout)
      : _root(std::move(root)), _layout(layout) {}

  /// false if there are no more ids
  bool next(IndexIterator::LocalDocumentIdCallback const& callback,
            std::uint64_t limit) {
    while (limit > 0) {
      if (!hasUnread() && !advance(/*smallRequest*/ limit < kReadBatch)) {
        return false;
      }
      std::size_t const n = _part->reader.read(std::span(_buffer).first(
          static_cast<std::size_t>(std::min(limit, kReadBatch))));
      limit -= n;
      for (std::uint32_t offset : std::span(_buffer).first(n)) {
        callback(LocalDocumentId::create(
            inverted_index::makeId(_layout, _part->shard, offset)));
      }
    }
    return true;
  }

  std::uint64_t skip(std::uint64_t count) {
    std::uint64_t skipped = 0;
    while (skipped < count &&
           (hasUnread() || advance(/*smallRequest*/ false))) {
      skipped += _part->reader.skip(count - skipped);
    }
    return skipped;
  }

 private:
  /// a computed part of the result, all in one shard
  struct Part {
    Part(std::vector<BitmapRef> bitmaps, std::uint64_t shard,
         std::uint32_t from)
        : reader(std::move(bitmaps), from), shard(shard) {}
    ShardReader reader;
    std::uint64_t shard;
  };

  void setPart(BitmapRef::Bitmap const* range, std::uint64_t shard,
               std::uint32_t from) {
    // the old part may borrow bitmaps that the new one replaces
    _part.reset();
    std::vector<BitmapRef> bitmaps = _root->parts(range);
    if (bitmaps.size() > ShardReader::kMaxMergedInputs) {
      BitmapRef united = unite(bitmaps);
      bitmaps.clear();
      bitmaps.push_back(std::move(united));
    }
    _part.emplace(std::move(bitmaps), shard, from);
  }

  bool hasUnread() const {
    return _part.has_value() && !_part->reader.exhausted();
  }

  /// Makes the next non-empty part of the result current; false if there is
  /// none. For a small first request on a large result, the first chunk that
  /// can hold a match comes first, and the rest of the shard is computed only
  /// if the caller reads on. A small LIMIT then does not pay for a union or
  /// intersection over the whole collection.
  bool advance(bool smallRequest) {
    do {
      if (_restFrom.has_value()) {
        std::uint32_t const from = *std::exchange(_restFrom, std::nullopt);
        setPart(nullptr, _part->shard, from);
        continue;
      }
      _part.reset();
      if (!_root->seek(_nextShard)) {
        return false;
      }
      std::uint64_t const shard = _root->shard();
      _nextShard = shard + 1;
      if (smallRequest &&
          _root->cost() > static_cast<std::int64_t>(kChunkSize)) {
        std::uint64_t const chunkStart =
            _root->minimum() / kChunkSize * kChunkSize;
        std::uint64_t const chunkEnd = chunkStart + kChunkSize;
        // the rest of the shard must start at an offset that fits 32 bits
        if (chunkEnd <= std::numeric_limits<std::uint32_t>::max()) {
          BitmapRef chunk = BitmapRef::owned(
              roaring::api::roaring_bitmap_from_range(chunkStart, chunkEnd, 1));
          setPart(chunk.get(), shard, 0);
          _restFrom = static_cast<std::uint32_t>(chunkEnd);
          continue;
        }
      }
      setPart(nullptr, shard, 0);
    } while (!hasUnread());
    return true;
  }

  std::unique_ptr<ShardNode> _root;
  inverted_index::IdLayout const _layout;
  /// may borrow bitmaps of the nodes in the tree
  std::optional<Part> _part;
  /// the current part is only the first chunk of its shard; the rest starts
  /// at this offset
  std::optional<std::uint32_t> _restFrom;
  std::uint64_t _nextShard = 0;
  /// a member, so that a rearm does not allocate it again
  std::array<std::uint32_t, kReadBatch> _buffer;
};

/// Checks every document of the collection, for conditions that match
/// documents in no bitmap, e.g. an empty array with ALL IN.
class DocumentScan {
 public:
  DocumentScan(RocksDBTransactionMethods& methods, bool readOwnWrites,
               std::uint64_t collectionObjectId, Expr expr,
               std::vector<basics::AttributeName> const& field)
      : _bounds(RocksDBKeyBounds::CollectionDocuments(collectionObjectId)),
        _upper(_bounds.end()),
        _expr(std::move(expr)),
        _field(field) {
    _it =
        methods.NewIterator(_bounds.columnFamily(), [&](ReadOptions& options) {
          options.iterate_upper_bound = &_upper;
          options.readOwnWrites = readOwnWrites;
        });
    _it->Seek(_bounds.start());
  }

  // the iterator points to _upper
  DocumentScan(DocumentScan const&) = delete;
  DocumentScan& operator=(DocumentScan const&) = delete;

  /// false if there are no more documents
  bool next(IndexIterator::LocalDocumentIdCallback const& callback,
            std::uint64_t limit) {
    for (; limit > 0 && _it->Valid(); _it->Next()) {
      velocypack::Slice doc(
          reinterpret_cast<uint8_t const*>(_it->value().data()));
      if (matches(doc)) {
        callback(RocksDBKey::documentId(_it->key()));
        --limit;
      }
    }
    if (!_it->Valid()) {
      rocksutils::checkIteratorStatus(*_it);
      return false;
    }
    return true;
  }

 private:
  bool matches(velocypack::Slice doc) const {
    auto terms = inverted_index::indexedTerms(doc, _field);
    return terms.ok() && inverted_index::matches(_expr, terms.get());
  }

  RocksDBKeyBounds const _bounds;
  rocksdb::Slice const _upper;
  Expr const _expr;
  std::vector<basics::AttributeName> const& _field;
  std::unique_ptr<rocksdb::Iterator> _it;
};

/// own writes are only visible to RocksDB once they are flushed
void flushPendingWrites(RocksDBTransactionMethods& methods) {
  auto* pending = methods.invertedIndexWrites();
  if (pending == nullptr || pending->empty()) {
    return;
  }
  if (rocksdb::Status s = pending->flush(methods); !s.ok()) {
    THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(s));
  }
}

class RocksDBInvertedIndexIterator final : public IndexIterator {
 public:
  RocksDBInvertedIndexIterator(LogicalCollection* collection,
                               transaction::Methods* trx,
                               RocksDBInvertedIndex const& index,
                               aql::AstNode const* node,
                               aql::Variable const* reference,
                               ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _index(index),
        _reference(reference),
        _iterators([this] {
          return methods().NewIterator(
              _index.columnFamily(), [this](ReadOptions& options) {
                options.readOwnWrites = readsOwnWrites();
              });
        }) {
    setCondition(node);
  }

  std::string_view typeName() const noexcept override {
    return "rocksdb-inverted-index-iterator";
  }

  bool canRearm() const override { return true; }

 protected:
  bool rearmImpl(aql::AstNode const* node, aql::Variable const* /*variable*/,
                 IndexIteratorOptions const& /*opts*/) override {
    setCondition(node);
    return true;
  }

  void resetImpl() override { startReading(); }

  /// A RocksDB inverted index covers no attributes. Covering reads still serve
  /// the batch materialization, which only needs the document ids in ascending
  /// order.
  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    SliceCoveringData covering(velocypack::Slice::emptyArraySlice());
    return nextImpl([&](LocalDocumentId id) { return callback(id, covering); },
                    limit);
  }

  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    return std::visit(
        overload{
            [](std::monostate) { return false; },
            [&](auto& reader) { return reader.next(callback, limit); },
        },
        _reader);
  }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    std::visit(
        overload{
            [](std::monostate) {},
            [&](BitmapCursor& cursor) { skipped += cursor.skip(count); },
            [&](DocumentScan&) { IndexIterator::skipImpl(count, skipped); },
        },
        _reader);
  }

 private:
  RocksDBTransactionMethods& methods() const {
    return *RocksDBTransactionState::toMethods(_trx, _collection->id());
  }

  bool readsOwnWrites() const {
    return canReadOwnWrites() == ReadOwnWrites::yes;
  }

  void setCondition(aql::AstNode const* node) {
    if (readsOwnWrites()) {
      flushPendingWrites(methods());
    }
    auto parts = inverted_index::matchCondition(_index, node, _reference,
                                                /*runtime*/ true);
    if (!parts.has_value()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "unsupported RocksDB inverted index condition");
    }
    _expr = inverted_index::normalize(parts->expr);
    _folded = inverted_index::fold(_expr);
    startReading();
  }

  void startReading() {
    // the old reader may view values of the pooled iterators
    _reader.emplace<std::monostate>();
    if (_folded.type == Expr::Type::False) {
      return;
    }
    if (inverted_index::needsScan(_folded)) {
      auto* physical =
          static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
      _reader.emplace<DocumentScan>(methods(), readsOwnWrites(),
                                    physical->objectId(), _expr,
                                    _index.fields()[0]);
      return;
    }
    _iterators.reset();
    _reader.emplace<BitmapCursor>(
        buildTree(_folded, _index.objectId(), _iterators), _index.idLayout());
  }

  RocksDBInvertedIndex const& _index;
  aql::Variable const* _reference;
  /// the condition with searched values normalized; a scan evaluates it
  Expr _expr;
  /// the condition with constants folded; the bitmaps answer it
  Expr _folded;
  IteratorPool _iterators;
  /// declared after the iterators, so that it is destroyed before them
  std::variant<std::monostate, BitmapCursor, DocumentScan> _reader;
};

}  // namespace

std::unique_ptr<IndexIterator> makeRocksDBInvertedIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    RocksDBInvertedIndex const& index, aql::AstNode const* node,
    aql::Variable const* reference, ReadOwnWrites readOwnWrites) {
  return std::make_unique<RocksDBInvertedIndexIterator>(
      collection, trx, index, node, reference, readOwnWrites);
}

}  // namespace arangodb
