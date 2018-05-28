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

#ifndef ARANGOD_AQL_AGGREGATOR_H
#define ARANGOD_AQL_AGGREGATOR_H 1

#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/AqlValue.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <set>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class MemoryBlockAllocator {
 public:
  /// @brief create a temporary storage instance
  explicit MemoryBlockAllocator(size_t blockSize)
      : blocks(), blockSize(blockSize), current(nullptr), end(nullptr) {}

  /// @brief destroy a temporary storage instance
  ~MemoryBlockAllocator() {
    clear();
  }
    
  void clear() {
    for (auto& it : blocks) {
      delete[] it;
    }
    blocks.clear();
    current = nullptr;
    end = nullptr;
  }

  /// @brief register a short string
  char* store(char const* p, size_t length) {
    if (current == nullptr || (current + length > end)) {
      allocateBlock();
    }

    TRI_ASSERT(!blocks.empty());
    TRI_ASSERT(current != nullptr);
    TRI_ASSERT(end != nullptr);
    TRI_ASSERT(current + length <= end);

    char* position = current;
    memcpy(static_cast<void*>(position), p, length);
    current += length;
    return position;
  }

 private:
  /// @brief allocate a new block of memory
  void allocateBlock() {
    char* buffer = new char[blockSize];

    try {
      blocks.emplace_back(buffer);
    } catch (...) {
      delete[] buffer;
      throw;
    }
    current = buffer;
    end = current + blockSize;
  }
 
  /// @brief already allocated blocks
  std::vector<char*> blocks;

  /// @brief size of each block
  size_t const blockSize;

  /// @brief offset into current block
  char* current;

  /// @brief end of current block
  char* end;
};

struct Aggregator {
  Aggregator() = delete;
  Aggregator(Aggregator const&) = delete;
  Aggregator& operator=(Aggregator const&) = delete;

  explicit Aggregator(transaction::Methods* trx) : trx(trx) {}
  virtual ~Aggregator() = default;
  virtual char const* name() const = 0;
  virtual void reset() = 0;
  virtual void reduce(AqlValue const&) = 0;
  virtual AqlValue stealValue() = 0;

  static std::unique_ptr<Aggregator> fromTypeString(transaction::Methods*,
                                                    std::string const&);
  static std::unique_ptr<Aggregator> fromVPack(transaction::Methods*,
                                               arangodb::velocypack::Slice const&, char const*);

  static bool isSupported(std::string const&);
  static bool requiresInput(std::string const&);

  transaction::Methods* trx;

  arangodb::velocypack::Builder builder;
};

struct AggregatorLength final : public Aggregator {
  explicit AggregatorLength(transaction::Methods* trx)
      : Aggregator(trx), count(0) {}
  AggregatorLength(transaction::Methods* trx, uint64_t initialCount)
      : Aggregator(trx), count(initialCount) {}

  char const* name() const override final { return "LENGTH"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  uint64_t count;
};

struct AggregatorMin final : public Aggregator {
  explicit AggregatorMin(transaction::Methods* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMin();

  char const* name() const override final { return "MIN"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  AqlValue value;
};

struct AggregatorMax final : public Aggregator {
  explicit AggregatorMax(transaction::Methods* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMax();

  char const* name() const override final { return "MAX"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  AqlValue value;
};

struct AggregatorSum final : public Aggregator {
  explicit AggregatorSum(transaction::Methods* trx)
      : Aggregator(trx), sum(0.0), invalid(false) {}

  char const* name() const override final { return "SUM"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  double sum;
  bool invalid;
};

struct AggregatorAverage final : public Aggregator {
  explicit AggregatorAverage(transaction::Methods* trx)
      : Aggregator(trx), count(0), sum(0.0), invalid(false) {}

  char const* name() const override final { return "AVERAGE"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  uint64_t count;
  double sum;
  bool invalid;
};

struct AggregatorVarianceBase : public Aggregator {
  AggregatorVarianceBase(transaction::Methods* trx, bool population)
      : Aggregator(trx),
        population(population),
        count(0),
        sum(0.0),
        mean(0.0),
        invalid(false) {}

  void reset() override final;
  void reduce(AqlValue const&) override final;

  bool const population;
  uint64_t count;
  double sum;
  double mean;
  bool invalid;
};

struct AggregatorVariance final : public AggregatorVarianceBase {
  AggregatorVariance(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  char const* name() const override final {
    if (population) {
      return "VARIANCE_POPULATION";
    }
    return "VARIANCE_SAMPLE";
  }

  AqlValue stealValue() override final;
};

struct AggregatorStddev final : public AggregatorVarianceBase {
  AggregatorStddev(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  char const* name() const override final {
    if (population) {
      return "STDDEV_POPULATION";
    }
    return "STDDEV_SAMPLE";
  }

  AqlValue stealValue() override final;
};

struct AggregatorUnique final : public Aggregator {
  explicit AggregatorUnique(transaction::Methods* trx, bool isArrayInput);

  ~AggregatorUnique();

  char const* name() const override final { return "UNIQUE"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
  bool isArrayInput;
};

struct AggregatorSortedUnique final : public Aggregator {
  explicit AggregatorSortedUnique(transaction::Methods* trx, bool isArrayInput);

  ~AggregatorSortedUnique();

  char const* name() const override final { return "SORTED_UNIQUE"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  MemoryBlockAllocator allocator;
  std::set<velocypack::Slice, basics::VelocyPackHelper::VPackLess<true>> seen;
  bool isArrayInput;
};

struct AggregatorCountDistinct final : public Aggregator {
  explicit AggregatorCountDistinct(transaction::Methods* trx, bool isArrayInput);

  ~AggregatorCountDistinct();

  char const* name() const override final { return "COUNT_DISTINCT"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
  bool isArrayInput;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
