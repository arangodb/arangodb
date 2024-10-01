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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

// In order to implement a new IndexIterator the following functions need to be
// implemented.
//
// typeName() returns a string descibing the type of the indexIterator
//
// The next() function of the IndexIterator expects a callback taking
// LocalDocumentIds that are created from RevisionIds. In addition it expects a
// limit. The iterator has to walk through the Index and call the callback with
// at most limit many elements. On the next iteration it has to continue after
// the last returned Token.
//
// reset() resets the iterator
//
// optional - default implementation provided:
//
// skip(trySkip, skipped) tries to skip the next trySkip elements
//
// When finished you need to implement the fuction:
//    virtual IndexIterator* iteratorForCondition(...)
// So a there is a way to create an iterator for the index

#pragma once

#include <iosfwd>
#include "Aql/AqlValue.h"
#include "Basics/debugging.h"
#include "Containers/FlatHashMap.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <function2/function2.hpp>
#include <velocypack/Slice.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-common.h>

namespace arangodb {
class Index;
class LogicalCollection;

namespace aql {
struct AstNode;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

struct IndexIteratorOptions;
enum class ReadOwnWrites : bool;

class IndexIteratorCoveringData {
 public:
  virtual ~IndexIteratorCoveringData() = default;
  virtual VPackSlice at(size_t i) const = 0;
  virtual bool isArray() const noexcept = 0;
  virtual VPackSlice value() const {
    // Only some "projections" are not accessed by index, but directly by value.
    // Like edge or primaryKey index. In general this method should not be
    // called for indexes providing projections as "array-like" structure.
    TRI_ASSERT(false);
    return VPackSlice::noneSlice();
  }

  virtual velocypack::ValueLength length() const = 0;
};

std::ostream& operator<<(std::ostream&, IndexIteratorCoveringData const&);

/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
class IndexIterator {
 public:
  class SliceCoveringData final : public IndexIteratorCoveringData {
   public:
    explicit SliceCoveringData(VPackSlice slice) : _slice(slice) {}

    VPackSlice at(size_t i) const override {
      TRI_ASSERT(_slice.isArray());
      return _slice.at(i);
    }

    VPackSlice value() const override { return _slice; }

    bool isArray() const noexcept override { return _slice.isArray(); }

    velocypack::ValueLength length() const override { return _slice.length(); }

   private:
    VPackSlice _slice;
  };

  class SliceCoveringDataWithStoredValues final
      : public IndexIteratorCoveringData {
   public:
    SliceCoveringDataWithStoredValues(VPackSlice slice, VPackSlice storedValues)
        : _slice(slice),
          _storedValues(storedValues),
          _sliceLength(slice.length()),
          _storedValuesLength(storedValues.length()) {}

    VPackSlice at(size_t i) const override {
      if (i >= _sliceLength) {
        TRI_ASSERT(_storedValues.isArray());
        return _storedValues.at(i - _sliceLength);
      }
      TRI_ASSERT(_slice.isArray());
      return _slice.at(i);
    }

    // should not be called in our case
    VPackSlice value() const override {
      TRI_ASSERT(false);
      return VPackSlice::noneSlice();
    }

    bool isArray() const noexcept override { return true; }

    velocypack::ValueLength length() const override {
      return _sliceLength + _storedValuesLength;
    }

   private:
    VPackSlice _slice;
    VPackSlice _storedValues;
    velocypack::ValueLength _sliceLength;
    velocypack::ValueLength _storedValuesLength;
  };

  // TODO(MBkkt) move callback stuff to separate header
  // TODO(MBkkt) try to use view/move_only options
  template<typename... Funcs>
  class CallbackImplStrict : fu2::function<Funcs...> {
    using Base = fu2::function<Funcs...>;

   public:
    using Base::operator();
    using Base::operator bool;

    bool operator==(std::nullptr_t) { return !bool(*this); }

    bool operator!=(std::nullptr_t) { return bool(*this); }

    CallbackImplStrict() noexcept = default;

    template<typename... Fs>
    CallbackImplStrict(Fs&&... fs)
        : Base{fu2::overload(std::forward<Fs>(fs)...)} {}
  };

  template<typename... Funcs>
  class CallbackImplWeak : CallbackImplStrict<Funcs...> {
    using Base = CallbackImplStrict<Funcs...>;

    struct DummyRetval {
      template<typename T>
      operator T() const {
        TRI_ASSERT(false);
        throw std::bad_function_call{};
      }
    };

   public:
    using Base::operator();
    using Base::operator bool;

    bool operator==(std::nullptr_t) { return !bool(*this); }

    bool operator!=(std::nullptr_t) { return bool(*this); }

    CallbackImplWeak() noexcept = default;

    template<typename... Fs>
    CallbackImplWeak(Fs&&... fs)
        : Base{std::forward<Fs>(fs)...,
               [](auto&&...) { return DummyRetval{}; }} {}
  };

  using LocalDocumentIdCallback =
      fu2::function<bool(LocalDocumentId token) const>;

  // the bool return value of the callback indicates whether the callback
  // has ignored or used the document.
  // if the callback returns true, it means the callback has done something
  // meaningful with the document, e.g. used it to write a result row into
  // some output block in AQL. the caller can then use the returned true
  // value as an indicator to count up the number of rows produced.
  // if the callback returns false, it means the callback has ignored the
  // document. this is true for example for callbacks that actively filter
  // out certain documents.
  using DocumentCallback = fu2::function<bool(
      LocalDocumentId token, aql::DocumentData&& data, VPackSlice doc) const>;

  static DocumentCallback makeDocumentCallback(velocypack::Builder& builder) {
    return [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
      builder.add(doc);
      return true;
    };
  }

  using CoveringCallback =
      CallbackImplWeak<bool(LocalDocumentId token,
                            IndexIteratorCoveringData& covering) const,
                       bool(aql::AqlValue&& searchDoc,
                            IndexIteratorCoveringData& covering) const>;

 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;
  IndexIterator() = delete;

  IndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                ReadOwnWrites readOwnWrites);

  virtual ~IndexIterator() = default;

  /// @brief return the underlying collection
  /// this is guaranteed to be a non-nullptr
  LogicalCollection* collection() const noexcept { return _collection; }

  transaction::Methods* transaction() const noexcept { return _trx; }

  bool hasMore() const noexcept { return _hasMore; }

  void reset() {
    // intentionally do not reset cache statistics here.
    _hasMore = true;
    resetImpl();
  }

  /// @brief Calls cb for the next batchSize many elements
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool next(IndexIterator::LocalDocumentIdCallback const& callback,
            uint64_t batchSize) {
    _hasMore = _hasMore && nextImpl(callback, batchSize);
    return _hasMore;
  }

  /// @brief Calls cb for the next batchSize many elements, complete documents
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextDocument(IndexIterator::DocumentCallback const& callback,
                    uint64_t batchSize) {
    _hasMore = _hasMore && nextDocumentImpl(callback, batchSize);
    return _hasMore;
  }

  /// @brief Calls cb for the next batchSize many elements, index-only
  /// projections returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextCovering(IndexIterator::CoveringCallback const& callback,
                    uint64_t batchSize) {
    _hasMore = _hasMore && nextCoveringImpl(callback, batchSize);
    return _hasMore;
  }

  /// @brief convenience function to retrieve all results
  void all(IndexIterator::LocalDocumentIdCallback const& callback) {
    while (next(callback, internalBatchSize)) {
      // intentionally empty
    }
  }

  /// @brief convenience function to retrieve all results
  void allDocuments(IndexIterator::DocumentCallback const& callback) {
    while (nextDocument(callback, internalBatchSize)) {
      // intentionally empty
    }
  }

  /// @brief convenience function to retrieve all results from a covering
  /// index
  void allCovering(IndexIterator::CoveringCallback const& callback) {
    while (nextCovering(callback, internalBatchSize)) {
      // intentionally empty
    }
  }

  /// @brief rearm the iterator with a new condition
  /// requires that canRearm() is true!
  /// if returns true it means that rearming has worked, and the iterator
  /// is ready to be used
  /// if returns false it means that the iterator does not support
  /// the provided condition and would only produce an empty result
  [[nodiscard]] bool rearm(arangodb::aql::AstNode const* node,
                           arangodb::aql::Variable const* variable,
                           IndexIteratorOptions const& opts) {
    TRI_ASSERT(canRearm());
    // intentionally do not reset cache statistics here.
    _hasMore = true;
    if (rearmImpl(node, variable, opts)) {
      reset();
      return true;
    }
    return false;
  }

  [[nodiscard]] bool rearm(velocypack::Slice slice,
                           IndexIteratorOptions const& opts) {
    TRI_ASSERT(canRearm());
    // intentionally do not reset cache statistics here.
    _hasMore = true;
    if (rearmImpl(slice, opts)) {
      reset();
      return true;
    }
    return false;
  }

  // set an optional limit for the index iterator.
  // the default implementation does nothing. derived classes can override it
  // as a performance optimization, so that fewer index results will be
  // produced.
  virtual void setLimit(uint64_t limit) noexcept {}

  /// @brief skip the next toSkip many elements.
  ///        skipped will be increased by the amount of skipped elements
  ///        afterwards Check hasMore()==true before using this NOTE: This will
  ///        throw on OUT_OF_MEMORY
  void skip(uint64_t toSkip, uint64_t& skipped);

  /// @brief skip all elements.
  ///        skipped will be increased by the amount of skipped elements
  ///        afterwards Check hasMore()==true before using this NOTE: This will
  ///        throw on OUT_OF_MEMORY
  void skipAll(uint64_t& skipped);

  virtual std::string_view typeName() const noexcept = 0;

  /// @brief whether or not the index iterator supports rearming
  virtual bool canRearm() const { return false; }

  /// @brief returns cache hits (first) and misses (second) statistics, and
  /// resets their values to 0
  [[nodiscard]] std::pair<std::uint64_t, std::uint64_t>
  getAndResetCacheStats() noexcept;

 protected:
  ReadOwnWrites canReadOwnWrites() const noexcept { return _readOwnWrites; }

  virtual bool rearmImpl(arangodb::aql::AstNode const* node,
                         arangodb::aql::Variable const* variable,
                         IndexIteratorOptions const& opts);

  virtual bool rearmImpl(velocypack::Slice slice,
                         IndexIteratorOptions const& opts);

  virtual bool nextImpl(LocalDocumentIdCallback const& callback,
                        uint64_t limit);
  virtual bool nextDocumentImpl(DocumentCallback const& callback,
                                uint64_t limit);

  // extract index attribute values directly from the index while index scanning
  virtual bool nextCoveringImpl(CoveringCallback const& callback,
                                uint64_t limit);

  virtual void resetImpl() {}

  virtual void skipImpl(uint64_t count, uint64_t& skipped);

  void incrCacheHits(std::uint64_t value = 1) noexcept { _cacheHits += value; }
  void incrCacheMisses(std::uint64_t value = 1) noexcept {
    _cacheMisses += value;
  }
  void incrCacheStats(bool found, std::uint64_t value = 1) noexcept {
    if (found) {
      _cacheHits += value;
    } else {
      _cacheMisses += value;
    }
  }

  // the default batch size is 1000, i.e. up to 1000 elements will be
  // fetched from an index in one go. this is an arbitrary value selected
  // in the early days of ArangoDB and has proven to work since then.
  static constexpr uint64_t internalBatchSize = 1000;

  LogicalCollection* _collection;
  transaction::Methods* _trx;

  // statistics
  std::uint64_t _cacheHits;
  std::uint64_t _cacheMisses;

  bool _hasMore;

 private:
  ReadOwnWrites const _readOwnWrites;
};

/// @brief Special iterator if the condition cannot have any result
class EmptyIndexIterator final : public IndexIterator {
 public:
  EmptyIndexIterator(LogicalCollection* collection, transaction::Methods* trx)
      : IndexIterator(collection, trx, ReadOwnWrites::no) {}

  ~EmptyIndexIterator() = default;

  std::string_view typeName() const noexcept override {
    return "empty-index-iterator";
  }

  bool nextImpl(LocalDocumentIdCallback const&, uint64_t) override {
    return false;
  }
  bool nextDocumentImpl(DocumentCallback const&, uint64_t) override {
    return false;
  }
  bool nextCoveringImpl(CoveringCallback const&, uint64_t) override {
    return false;
  }

  void resetImpl() override { _hasMore = false; }

  void skipImpl(uint64_t /*count*/, uint64_t& skipped) override { skipped = 0; }
};

/// Options for creating an index iterator
struct IndexIteratorOptions {
  /// @brief Limit used in a parent LIMIT node (if non-zero)
  size_t limit = 0;
  /// @brief number of lookahead elements considered before computing the next
  /// intersection of the Z-curve with the search range
  size_t lookahead = 1;
  /// @brief whether the index must sort its results
  bool sorted = true;
  /// @brief the index sort order - this is the same order for all indexes
  bool ascending = true;
  /// @brief Whether FCalls will be evaluated entirely or just it's arguments
  /// Used when creating the condition required to build an iterator
  bool evaluateFCalls = true;
  /// @brief enable caching
  bool useCache = true;
  /// @brief forcefully synchronize external indexes
  bool waitForSync = false;
  /// @brief iterator will be used with late materialization
  bool forLateMaterialization{false};
};

/// index estimate map, defined here because it was convenient
using IndexEstMap = containers::FlatHashMap<std::string, double>;
}  // namespace arangodb
