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

#include "Aggregator.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <set>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {

constexpr bool doesRequireInput = true;
constexpr bool doesNotRequireInput = false;

constexpr bool official = true;
constexpr bool internalOnly = false;

/// @brief struct containing aggregator meta information
struct AggregatorInfo {
  /// @brief function to generate a new aggregator instance in a query
  std::function<std::unique_ptr<Aggregator>(transaction::Methods* trx)> generator;

  /// @brief whether or not the aggregator needs input
  /// note: currently this is false only for LENGTH/COUNT, for which the input
  /// is optimized away
  bool requiresInput;

  /// @brief whether or not the aggregator is part of the public API (i.e.
  /// callable from a user AQL query)
  bool isOfficialAggregator;

  /// @brief under which name this aggregator is pushed on a DB server. if left
  /// empty, the aggregator will only work on a coordinator and not be pushed
  /// onto a DB server for example, the SUM aggregator is cumutative, so it can
  /// be pushed executed on partial results on the DB server as SUM too however,
  /// the LENGTH aggregator needs to be pushed as LENGTH to the DB server, but
  /// the partial lengths need to be aggregated on the coordinator using SUM
  std::string pushToDBServerAs;

  /// @brief under which name this aggregator is executed on a coordinator to
  /// aggregate partial results from the DB servers if parts of the aggregation
  /// were pushed to the DB servers for example, the LENGTH aggregator will be
  /// pushed to the DB server as LENGTH, but on the coordinator the aggregator
  /// needs to be converted to SUM to sum up the partial lengths from the DB
  /// servers
  std::string runOnCoordinatorAs;
};

/// @brief helper class for block-wise memory allocations
class MemoryBlockAllocator {
 public:
  /// @brief create a temporary storage instance
  explicit MemoryBlockAllocator(size_t blockSize)
      : blockSize(blockSize), current(nullptr), end(nullptr) {}

  /// @brief destroy a temporary storage instance
  ~MemoryBlockAllocator() { clear(); }

  void clear() noexcept {
    for (auto& it : blocks) {
      delete[] it;
    }
    blocks.clear();
    current = nullptr;
    end = nullptr;
  }

  /// @brief register a short data value
  char* store(char const* p, size_t length) {
    if (current == nullptr || (current + length > end)) {
      allocateBlock(length);
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
  void allocateBlock(size_t minLength) {
    size_t length = std::max(minLength, blockSize);
    char* buffer = new char[length];

    try {
      blocks.emplace_back(buffer);
    } catch (...) {
      delete[] buffer;
      throw;
    }
    current = buffer;
    end = current + length;
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

/// @brief aggregator for LENGTH()
struct AggregatorLength final : public Aggregator {
  explicit AggregatorLength(transaction::Methods* trx)
      : Aggregator(trx), count(0) {}
  AggregatorLength(transaction::Methods* trx, uint64_t initialCount)
      : Aggregator(trx), count(initialCount) {}

  void reset() override { count = 0; }

  void reduce(AqlValue const&) override { ++count; }

  AqlValue stealValue() override {
    uint64_t value = count;
    reset();
    return AqlValue(AqlValueHintUInt(value));
  }

  uint64_t count;
};

struct AggregatorMin final : public Aggregator {
  explicit AggregatorMin(transaction::Methods* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMin() { value.destroy(); }

  void reset() override { value.erase(); }

  void reduce(AqlValue const& cmpValue) override {
    if (value.isEmpty() || (!cmpValue.isNull(true) &&
                            AqlValue::Compare(trx, value, cmpValue, true) > 0)) {
      // the value `null` itself will not be used in MIN() to compare lower than
      // e.g. value `false`
      value.destroy();
      value = cmpValue.clone();
    }
  }

  AqlValue stealValue() override {
    if (value.isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    AqlValue copy = value;
    value.erase();
    return copy;
  }

  AqlValue value;
};

struct AggregatorMax final : public Aggregator {
  explicit AggregatorMax(transaction::Methods* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMax() { value.destroy(); }

  void reset() override { value.erase(); }

  void reduce(AqlValue const& cmpValue) override {
    if (value.isEmpty() || AqlValue::Compare(trx, value, cmpValue, true) < 0) {
      value.destroy();
      value = cmpValue.clone();
    }
  }

  AqlValue stealValue() override {
    if (value.isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    AqlValue copy = value;
    value.erase();
    return copy;
  }

  AqlValue value;
};

struct AggregatorSum final : public Aggregator {
  explicit AggregatorSum(transaction::Methods* trx)
      : Aggregator(trx), sum(0.0), invalid(false), invoked(false) {}

  void reset() override {
    sum = 0.0;
    invalid = false;
    invoked = false;
  }

  void reduce(AqlValue const& cmpValue) override {
    invoked = true;
    if (!invalid) {
      if (cmpValue.isNull(true)) {
        // ignore `null` values here
        return;
      }
      if (cmpValue.isNumber()) {
        double const number = cmpValue.toDouble(trx);
        if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
          sum += number;
          return;
        }
      }
    }

    invalid = true;
  }

  AqlValue stealValue() override {
    if (invalid || !invoked || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    double v = sum;
    reset();
    return AqlValue(AqlValueHintDouble(v));
  }

  double sum;
  bool invalid;
  bool invoked;
};

/// @brief the single-server variant of AVERAGE
struct AggregatorAverage : public Aggregator {
  explicit AggregatorAverage(transaction::Methods* trx)
      : Aggregator(trx), count(0), sum(0.0), invalid(false) {}

  void reset() override final {
    count = 0;
    sum = 0.0;
    invalid = false;
  }

  virtual void reduce(AqlValue const& cmpValue) override {
    if (!invalid) {
      if (cmpValue.isNull(true)) {
        // ignore `null` values here
        return;
      }
      if (cmpValue.isNumber()) {
        double const number = cmpValue.toDouble(trx);
        if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
          sum += number;
          ++count;
          return;
        }
      }
    }

    invalid = true;
  }

  virtual AqlValue stealValue() override {
    if (invalid || count == 0 || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    TRI_ASSERT(count > 0);

    double v = sum / count;
    reset();
    return AqlValue(AqlValueHintDouble(v));
  }

  uint64_t count;
  double sum;
  bool invalid;
};

/// @brief the DB server variant of AVERAGE, producing a sum and a count
struct AggregatorAverageStep1 final : public AggregatorAverage {
  explicit AggregatorAverageStep1(transaction::Methods* trx)
      : AggregatorAverage(trx) {}

  // special version that will produce an array with sum and count separately
  AqlValue stealValue() override {
    builder.clear();
    builder.openArray();
    if (invalid || count == 0 || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      builder.add(VPackValue(VPackValueType::Null));
      builder.add(VPackValue(VPackValueType::Null));
    } else {
      TRI_ASSERT(count > 0);
      builder.add(VPackValue(sum));
      builder.add(VPackValue(count));
    }
    builder.close();
    AqlValue temp(builder.slice());
    reset();
    return temp;
  }

  arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of AVERAGE, aggregating partial sums and
/// counts
struct AggregatorAverageStep2 final : public AggregatorAverage {
  explicit AggregatorAverageStep2(transaction::Methods* trx)
      : AggregatorAverage(trx) {}

  void reduce(AqlValue const& cmpValue) override {
    if (!cmpValue.isArray()) {
      invalid = true;
      return;
    }

    bool mustDestroy;
    AqlValue const& sumValue = cmpValue.at(trx, 0, mustDestroy, false);
    AqlValue const& countValue = cmpValue.at(trx, 1, mustDestroy, false);

    if (sumValue.isNull(true) || countValue.isNull(true)) {
      invalid = true;
      return;
    }

    bool failed = false;
    double v = sumValue.toDouble(trx, failed);
    if (failed) {
      invalid = true;
      return;
    }
    sum += v;
    count += countValue.toInt64(trx);
  }
};

/// @brief base functionality for VARIANCE
struct AggregatorVarianceBase : public Aggregator {
  AggregatorVarianceBase(transaction::Methods* trx, bool population)
      : Aggregator(trx), population(population), count(0), sum(0.0), mean(0.0), invalid(false) {}

  void reset() override {
    count = 0;
    sum = 0.0;
    mean = 0.0;
    invalid = false;
  }

  void reduce(AqlValue const& cmpValue) override {
    if (!invalid) {
      if (cmpValue.isNull(true)) {
        // ignore `null` values here
        return;
      }
      if (cmpValue.isNumber()) {
        double const number = cmpValue.toDouble(trx);
        if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
          double const delta = number - mean;
          ++count;
          mean += delta / count;
          sum += delta * (number - mean);
          return;
        }
      }
    }

    invalid = true;
  }

  bool const population;
  uint64_t count;
  double sum;
  double mean;
  bool invalid;
};

/// @brief the single server variant of VARIANCE
struct AggregatorVariance : public AggregatorVarianceBase {
  AggregatorVariance(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  AqlValue stealValue() override {
    if (invalid || count == 0 || (count == 1 && !population) ||
        std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    TRI_ASSERT(count > 0);

    double v;
    if (!population) {
      TRI_ASSERT(count > 1);
      v = sum / (count - 1);
    } else {
      v = sum / count;
    }

    reset();
    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the DB server variant of VARIANCE/STDDEV
struct AggregatorVarianceBaseStep1 final : public AggregatorVarianceBase {
  AggregatorVarianceBaseStep1(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  AqlValue stealValue() override {
    builder.clear();
    builder.openArray();
    if (invalid || count == 0 || (count == 1 && !population) ||
        std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      builder.add(VPackValue(VPackValueType::Null));
      builder.add(VPackValue(VPackValueType::Null));
      builder.add(VPackValue(VPackValueType::Null));
    } else {
      TRI_ASSERT(count > 0);

      builder.add(VPackValue(count));
      builder.add(VPackValue(sum));
      builder.add(VPackValue(mean));
    }
    builder.close();

    AqlValue temp(builder.slice());
    reset();
    return temp;
  }

  arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of VARIANCE
struct AggregatorVarianceBaseStep2 : public AggregatorVarianceBase {
  AggregatorVarianceBaseStep2(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  void reset() override {
    AggregatorVarianceBase::reset();
    values.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    if (!cmpValue.isArray()) {
      invalid = true;
      return;
    }

    bool mustDestroy;
    AqlValue const& countValue = cmpValue.at(trx, 0, mustDestroy, false);
    AqlValue const& sumValue = cmpValue.at(trx, 1, mustDestroy, false);
    AqlValue const& meanValue = cmpValue.at(trx, 2, mustDestroy, false);

    if (countValue.isNull(true) || sumValue.isNull(true) || meanValue.isNull(true)) {
      invalid = true;
      return;
    }

    bool failed = false;
    double v1 = sumValue.toDouble(trx, failed);
    if (failed) {
      invalid = true;
      return;
    }
    double v2 = meanValue.toDouble(trx, failed);
    if (failed) {
      invalid = true;
      return;
    }

    int64_t c = countValue.toInt64(trx);
    if (c == 0) {
      invalid = true;
      return;
    }
    count += c;
    sum += v2 * c;
    mean += v2 * c;
    values.emplace_back(std::make_tuple(v1 / c, v2, c));
  }

  AqlValue stealValue() override {
    if (invalid || count == 0 || (count == 1 && !population)) {
      return AqlValue(AqlValueHintNull());
    }

    double const avg = sum / count;
    double v = 0.0;
    for (auto const& it : values) {
      double variance = std::get<0>(it);
      double mean = std::get<1>(it);
      int64_t count = std::get<2>(it);
      v += count * (variance + std::pow(mean - avg, 2));
    }

    if (!population) {
      TRI_ASSERT(count > 1);
      v /= count - 1;
    } else {
      v /= count;
    }
    reset();
    return AqlValue(AqlValueHintDouble(v));
  }

  std::vector<std::tuple<double, double, int64_t>> values;
};

/// @brief the single server variant of STDDEV
struct AggregatorStddev : public AggregatorVarianceBase {
  AggregatorStddev(transaction::Methods* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  AqlValue stealValue() override {
    if (invalid || count == 0 || (count == 1 && !population) ||
        std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    TRI_ASSERT(count > 0);

    double v;
    if (!population) {
      TRI_ASSERT(count > 1);
      v = std::sqrt(sum / (count - 1));
    } else {
      v = std::sqrt(sum / count);
    }

    reset();
    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the coordinator variant of STDDEV
struct AggregatorStddevBaseStep2 final : public AggregatorVarianceBaseStep2 {
  AggregatorStddevBaseStep2(transaction::Methods* trx, bool population)
      : AggregatorVarianceBaseStep2(trx, population) {}

  AqlValue stealValue() override {
    if (invalid || count == 0 || (count == 1 && !population)) {
      return AqlValue(AqlValueHintNull());
    }

    double const avg = sum / count;
    double v = 0.0;
    for (auto const& it : values) {
      double variance = std::get<0>(it);
      double mean = std::get<1>(it);
      int64_t count = std::get<2>(it);
      v += count * (variance + std::pow(mean - avg, 2));
    }

    if (!population) {
      TRI_ASSERT(count > 1);
      v /= count - 1;
    } else {
      v /= count;
    }

    v = std::sqrt(v);
    reset();
    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the single-server and DB server variant of UNIQUE
struct AggregatorUnique : public Aggregator {
  explicit AggregatorUnique(transaction::Methods* trx)
      : Aggregator(trx),
        allocator(1024),
        seen(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(
                 trx->transactionContext()->getVPackOptions())) {}

  ~AggregatorUnique() { reset(); }

  void reset() override final {
    seen.clear();
    builder.clear();
    allocator.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);

    if (builder.isClosed()) {
      builder.openArray();
    }
    builder.add(VPackSlice(pos));
  }

  AqlValue stealValue() override final {
    // if not yet an array, start one
    if (builder.isClosed()) {
      builder.openArray();
    }

    // always close the Builder
    builder.close();
    AqlValue result(builder.slice());
    reset();
    return result;
  }

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
  arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of UNIQUE
struct AggregatorUniqueStep2 final : public AggregatorUnique {
  explicit AggregatorUniqueStep2(transaction::Methods* trx)
      : AggregatorUnique(trx) {}

  void reduce(AqlValue const& cmpValue) override final {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (auto const& it : VPackArrayIterator(s)) {
      if (seen.find(it) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(pos);

      if (builder.isClosed()) {
        builder.openArray();
      }
      builder.add(VPackSlice(pos));
    }
  }
};

/// @brief the single-server and DB server variant of SORTED_UNIQUE
struct AggregatorSortedUnique : public Aggregator {
  explicit AggregatorSortedUnique(transaction::Methods* trx)
      : Aggregator(trx),
        allocator(1024),
        seen(trx->transactionContext()->getVPackOptions()) {}

  ~AggregatorSortedUnique() { reset(); }

  void reset() override final {
    seen.clear();
    allocator.clear();
    builder.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);
  }

  AqlValue stealValue() override final {
    builder.openArray();
    for (auto const& it : seen) {
      builder.add(it);
    }

    // always close the Builder
    builder.close();
    AqlValue result(builder.slice());
    reset();
    return result;
  }

  MemoryBlockAllocator allocator;
  std::set<velocypack::Slice, basics::VelocyPackHelper::VPackLess<true>> seen;
  arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of SORTED_UNIQUE
struct AggregatorSortedUniqueStep2 final : public AggregatorSortedUnique {
  explicit AggregatorSortedUniqueStep2(transaction::Methods* trx)
      : AggregatorSortedUnique(trx) {}

  void reduce(AqlValue const& cmpValue) override final {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (auto const& it : VPackArrayIterator(s)) {
      if (seen.find(it) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(pos);
    }
  }
};

struct AggregatorCountDistinct : public Aggregator {
  explicit AggregatorCountDistinct(transaction::Methods* trx)
      : Aggregator(trx),
        allocator(1024),
        seen(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(
                 trx->transactionContext()->getVPackOptions())) {}

  ~AggregatorCountDistinct() { reset(); }

  void reset() override final {
    seen.clear();
    allocator.clear();
    builder.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);
  }

  AqlValue stealValue() override final {
    uint64_t value = seen.size();
    reset();
    return AqlValue(AqlValueHintUInt(value));
  }

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
  arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of COUNT_DISTINCT
struct AggregatorCountDistinctStep2 final : public AggregatorCountDistinct {
  explicit AggregatorCountDistinctStep2(transaction::Methods* trx)
      : AggregatorCountDistinct(trx) {}

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(trx);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (auto const& it : VPackArrayIterator(s)) {
      if (seen.find(s) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(pos);
    }
  }
};

/// @brief all available aggregators with their meta data
std::unordered_map<std::string, AggregatorInfo> const aggregators = {
    {"LENGTH",
     {
         [](transaction::Methods* trx) {
           return std::make_unique<AggregatorLength>(trx);
         },
         doesNotRequireInput, official, "LENGTH",
         "SUM"  // we need to sum up the lengths from the DB servers
     }},
    {"MIN",
     {
         [](transaction::Methods* trx) {
           return std::make_unique<AggregatorMin>(trx);
         },
         doesRequireInput, official, "MIN",
         "MIN"  // min is commutative
     }},
    {"MAX",
     {
         [](transaction::Methods* trx) {
           return std::make_unique<AggregatorMax>(trx);
         },
         doesRequireInput, official, "MAX",
         "MAX"  // max is commutative
     }},
    {"SUM",
     {
         [](transaction::Methods* trx) {
           return std::make_unique<AggregatorSum>(trx);
         },
         doesRequireInput, official, "SUM",
         "SUM"  // sum is commutative
     }},
    {"AVERAGE",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorAverage>(trx);
      },
      doesRequireInput, official, "AVERAGE_STEP1", "AVERAGE_STEP2"}},
    {"AVERAGE_STEP1",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorAverageStep1>(trx);
      },
      doesRequireInput, internalOnly, "", "AVERAGE_STEP1"}},
    {"AVERAGE_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorAverageStep2>(trx);
      },
      doesRequireInput, internalOnly, "", "AVERAGE_STEP2"}},
    {"VARIANCE_POPULATION",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVariance>(trx, true);
      },
      doesRequireInput, official, "VARIANCE_POPULATION_STEP1",
      "VARIANCE_POPULATION_STEP2"}},
    {"VARIANCE_POPULATION_STEP1",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep1>(trx, true);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_POPULATION_STEP1"}},
    {"VARIANCE_POPULATION_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep2>(trx, true);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_POPULATION_STEP2"}},
    {"VARIANCE_SAMPLE",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVariance>(trx, false);
      },
      doesRequireInput, official, "VARIANCE_SAMPLE_STEP1",
      "VARIANCE_SAMPLE_STEP2"}},
    {"VARIANCE_SAMPLE_STEP1",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep1>(trx, false);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_SAMPLE_STEP1"}},
    {"VARIANCE_SAMPLE_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep2>(trx, false);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_SAMPLE_STEP2"}},
    {"STDDEV_POPULATION",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorStddev>(trx, true);
      },
      doesRequireInput, official, "STDDEV_POPULATION_STEP1",
      "STDDEV_POPULATION_STEP2"}},
    {"STDDEV_POPULATION_STEP1",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep1>(trx, true);
      },
      doesRequireInput, internalOnly, "", "STDDEV_POPULATION_STEP1"}},
    {"STDDEV_POPULATION_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorStddevBaseStep2>(trx, true);
      },
      doesRequireInput, internalOnly, "", "STDDEV_POPULATION_STEP2"}},
    {"STDDEV_SAMPLE",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorStddev>(trx, false);
      },
      doesRequireInput, official, "STDDEV_SAMPLE_STEP1", "STDDEV_SAMPLE_STEP2"}},
    {"STDDEV_SAMPLE_STEP1",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorVarianceBaseStep1>(trx, false);
      },
      doesRequireInput, internalOnly, "", "STDDEV_SAMPLE_STEP1"}},
    {"STDDEV_SAMPLE_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorStddevBaseStep2>(trx, false);
      },
      doesRequireInput, internalOnly, "", "STDDEV_SAMPLE_STEP2"}},
    {"UNIQUE",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorUnique>(trx);
      },
      doesRequireInput, official, "UNIQUE", "UNIQUE_STEP2"}},
    {"UNIQUE_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorUniqueStep2>(trx);
      },
      doesRequireInput, internalOnly, "", "UNIQUE_STEP2"}},
    {"SORTED_UNIQUE",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorSortedUnique>(trx);
      },
      doesRequireInput, official, "SORTED_UNIQUE", "SORTED_UNIQUE_STEP2"}},
    {"SORTED_UNIQUE_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorSortedUniqueStep2>(trx);
      },
      doesRequireInput, internalOnly, "", "SORTED_UNIQUE_STEP2"}},
    {"COUNT_DISTINCT",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorCountDistinct>(trx);
      },
      doesRequireInput, official, "UNIQUE", "COUNT_DISTINCT_STEP2"}},
    {"COUNT_DISTINCT_STEP2",
     {[](transaction::Methods* trx) {
        return std::make_unique<AggregatorCountDistinctStep2>(trx);
      },
      doesRequireInput, internalOnly, "", "COUNT_DISTINCT_STEP2"}}};

/// @brief aliases (user-visible) for aggregation functions
std::unordered_map<std::string, std::string> const aliases = {
    {"COUNT", "LENGTH"},                  // COUNT = LENGTH
    {"AVG", "AVERAGE"},                   // AVG = AVERAGE
    {"VARIANCE", "VARIANCE_POPULATION"},  // VARIANCE = VARIANCE_POPULATION
    {"STDDEV", "STDDEV_POPULATION"},      // STDDEV = STDDEV_POPULATION
    {"COUNT_UNIQUE", "COUNT_DISTINCT"}    // COUNT_UNIQUE = COUNT_DISTINCT
};

}  // namespace

std::unique_ptr<Aggregator> Aggregator::fromTypeString(transaction::Methods* trx,
                                                       std::string const& type) {
  // will always return a valid generator function or throw an exception
  auto generator = Aggregator::factoryFromTypeString(type);
  TRI_ASSERT(generator != nullptr);

  return (*generator)(trx);
}

std::unique_ptr<Aggregator> Aggregator::fromVPack(transaction::Methods* trx,
                                                  arangodb::velocypack::Slice const& slice,
                                                  char const* nameAttribute) {
  VPackSlice variable = slice.get(nameAttribute);

  if (variable.isString()) {
    return fromTypeString(trx, variable.copyString());
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const* Aggregator::factoryFromTypeString(
    std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it != ::aggregators.end()) {
    return &((*it).second.generator);
  }
  // aggregator function name should have been validated before
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

std::string Aggregator::translateAlias(std::string const& name) {
  auto it = ::aliases.find(name);

  if (it != ::aliases.end()) {
    return (*it).second;
  }
  // not an alias
  return name;
}

std::string Aggregator::pushToDBServerAs(std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it != ::aggregators.end()) {
    return (*it).second.pushToDBServerAs;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

std::string Aggregator::runOnCoordinatorAs(std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it != ::aggregators.end()) {
    return (*it).second.runOnCoordinatorAs;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

bool Aggregator::isValid(std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it == ::aggregators.end()) {
    return false;
  }
  // check if the aggregator is part of the public API or internal
  return (*it).second.isOfficialAggregator;
}

bool Aggregator::requiresInput(std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it != ::aggregators.end()) {
    return (*it).second.requiresInput;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}
