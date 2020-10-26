////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
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
  std::function<std::unique_ptr<Aggregator>(velocypack::Options const* opts)> generator;

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
  explicit AggregatorLength(velocypack::Options const* opts)
      : Aggregator(opts), count(0) {}
  AggregatorLength(velocypack::Options const* opts, uint64_t initialCount)
      : Aggregator(opts), count(initialCount) {}

  void reset() override { count = 0; }

  void reduce(AqlValue const&) override { ++count; }

  AqlValue get() const override {
    uint64_t value = count;
    return AqlValue(AqlValueHintUInt(value));
  }

  uint64_t count;
};

struct AggregatorMin final : public Aggregator {
  explicit AggregatorMin(velocypack::Options const* opts)
      : Aggregator(opts), value() {}

  ~AggregatorMin() { value.destroy(); }

  void reset() override { value.erase(); }

  void reduce(AqlValue const& cmpValue) override {
    if (!cmpValue.isNull(true) && (value.isEmpty() || 
                                   AqlValue::Compare(_vpackOptions, value, cmpValue, true) > 0)) {
      // the value `null` itself will not be used in MIN() to compare lower than
      // e.g. value `false`
      value.destroy();
      value = cmpValue.clone();
    }
  }

  AqlValue get() const override {
    if (value.isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    return value.clone();
  }

  AqlValue value;
};

struct AggregatorMax final : public Aggregator {
  explicit AggregatorMax(velocypack::Options const* opts)
      : Aggregator(opts), value() {}

  ~AggregatorMax() { value.destroy(); }

  void reset() override { value.erase(); }

  void reduce(AqlValue const& cmpValue) override {
    if (value.isEmpty() || AqlValue::Compare(_vpackOptions, value, cmpValue, true) < 0) {
      value.destroy();
      value = cmpValue.clone();
    }
  }

  AqlValue get() const override {
    if (value.isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    return value.clone();
  }

  AqlValue value;
};

struct AggregatorSum final : public Aggregator {
  explicit AggregatorSum(velocypack::Options const* opts)
      : Aggregator(opts), sum(0.0), invalid(false), invoked(false) {}

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
        double const number = cmpValue.toDouble();
        if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
          sum += number;
          return;
        }
      }
    }

    invalid = true;
  }

  AqlValue get() const override {
    if (invalid || !invoked || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    double v = sum;
    return AqlValue(AqlValueHintDouble(v));
  }

  double sum;
  bool invalid;
  bool invoked;
};

/// @brief the single-server variant of AVERAGE
struct AggregatorAverage : public Aggregator {
  explicit AggregatorAverage(velocypack::Options const* opts)
      : Aggregator(opts), count(0), sum(0.0), invalid(false) {}

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
        double const number = cmpValue.toDouble();
        if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
          sum += number;
          ++count;
          return;
        }
      }
    }

    invalid = true;
  }

  virtual AqlValue get() const override {
    if (invalid || count == 0 || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
      return AqlValue(AqlValueHintNull());
    }

    TRI_ASSERT(count > 0);

    double v = sum / count;
    return AqlValue(AqlValueHintDouble(v));
  }

  uint64_t count;
  double sum;
  bool invalid;
};

/// @brief the DB server variant of AVERAGE, producing a sum and a count
struct AggregatorAverageStep1 final : public AggregatorAverage {
  explicit AggregatorAverageStep1(velocypack::Options const* opts)
      : AggregatorAverage(opts) {}

  // special version that will produce an array with sum and count separately
  AqlValue get() const override {
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
    return AqlValue(builder.slice());
  }

  mutable arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of AVERAGE, aggregating partial sums and
/// counts
struct AggregatorAverageStep2 final : public AggregatorAverage {
  explicit AggregatorAverageStep2(velocypack::Options const* opts)
      : AggregatorAverage(opts) {}

  void reduce(AqlValue const& cmpValue) override {
    if (!cmpValue.isArray()) {
      invalid = true;
      return;
    }

    bool mustDestroy;
    AqlValue const& sumValue = cmpValue.at(0, mustDestroy, false);
    AqlValue const& countValue = cmpValue.at(1, mustDestroy, false);

    if (sumValue.isNull(true) || countValue.isNull(true)) {
      invalid = true;
      return;
    }

    bool failed = false;
    double v = sumValue.toDouble(failed);
    if (failed) {
      invalid = true;
      return;
    }
    sum += v;
    count += countValue.toInt64();
  }
};

/// @brief base functionality for VARIANCE
struct AggregatorVarianceBase : public Aggregator {
  AggregatorVarianceBase(velocypack::Options const* opts, bool population)
      : Aggregator(opts), population(population), count(0), sum(0.0), mean(0.0), invalid(false) {}

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
        double const number = cmpValue.toDouble();
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
  AggregatorVariance(velocypack::Options const* opts, bool population)
      : AggregatorVarianceBase(opts, population) {}

  AqlValue get() const override {
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

    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the DB server variant of VARIANCE/STDDEV
struct AggregatorVarianceBaseStep1 final : public AggregatorVarianceBase {
  AggregatorVarianceBaseStep1(velocypack::Options const* opts, bool population)
      : AggregatorVarianceBase(opts, population) {}

  AqlValue get() const override {
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

    return AqlValue(builder.slice());
  }

  mutable arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of VARIANCE
struct AggregatorVarianceBaseStep2 : public AggregatorVarianceBase {
  AggregatorVarianceBaseStep2(velocypack::Options const* opts, bool population)
      : AggregatorVarianceBase(opts, population) {}

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
    AqlValue const& countValue = cmpValue.at(0, mustDestroy, false);
    AqlValue const& sumValue = cmpValue.at(1, mustDestroy, false);
    AqlValue const& meanValue = cmpValue.at(2, mustDestroy, false);

    if (countValue.isNull(true) || sumValue.isNull(true) || meanValue.isNull(true)) {
      invalid = true;
      return;
    }

    bool failed = false;
    double v1 = sumValue.toDouble(failed);
    if (failed) {
      invalid = true;
      return;
    }
    double v2 = meanValue.toDouble(failed);
    if (failed) {
      invalid = true;
      return;
    }

    int64_t c = countValue.toInt64();
    if (c == 0) {
      invalid = true;
      return;
    }
    count += c;
    sum += v2 * c;
    mean += v2 * c;
    values.emplace_back(std::make_tuple(v1 / c, v2, c));
  }

  AqlValue get() const override {
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
    return AqlValue(AqlValueHintDouble(v));
  }

  std::vector<std::tuple<double, double, int64_t>> values;
};

/// @brief the single server variant of STDDEV
struct AggregatorStddev : public AggregatorVarianceBase {
  AggregatorStddev(velocypack::Options const* opts, bool population)
      : AggregatorVarianceBase(opts, population) {}

  AqlValue get() const override {
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

    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the coordinator variant of STDDEV
struct AggregatorStddevBaseStep2 final : public AggregatorVarianceBaseStep2 {
  AggregatorStddevBaseStep2(velocypack::Options const* opts, bool population)
      : AggregatorVarianceBaseStep2(opts, population) {}

  AqlValue get() const override {
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
    return AqlValue(AqlValueHintDouble(v));
  }
};

/// @brief the single-server and DB server variant of UNIQUE
struct AggregatorUnique : public Aggregator {
  explicit AggregatorUnique(velocypack::Options const* opts)
      : Aggregator(opts),
        allocator(1024),
        seen(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(_vpackOptions)) {}

  ~AggregatorUnique() {
    reset();
  }

  // cppcheck-suppress virtualCallInConstructor
  void reset() override final {
    seen.clear();
    builder.clear();
    allocator.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(reinterpret_cast<uint8_t const*>(pos));

    if (builder.isClosed()) {
      builder.openArray();
    }
    builder.add(VPackSlice(reinterpret_cast<uint8_t const*>(pos)));
  }

  AqlValue get() const override final {
    // if not yet an array, start one
    if (builder.isClosed()) {
      builder.openArray();
    }

    // always close the Builder
    builder.close();
    return AqlValue(builder.slice());
  }

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
  mutable arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of UNIQUE
struct AggregatorUniqueStep2 final : public AggregatorUnique {
  explicit AggregatorUniqueStep2(velocypack::Options const* opts)
      : AggregatorUnique(opts) {}

  void reduce(AqlValue const& cmpValue) override final {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (VPackSlice it : VPackArrayIterator(s)) {
      if (seen.find(it) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(reinterpret_cast<uint8_t const*>(pos));

      if (builder.isClosed()) {
        builder.openArray();
      }
      builder.add(VPackSlice(reinterpret_cast<uint8_t const*>(pos)));
    }
  }
};

/// @brief the single-server and DB server variant of SORTED_UNIQUE
struct AggregatorSortedUnique : public Aggregator {
  explicit AggregatorSortedUnique(velocypack::Options const* opts)
      : Aggregator(opts),
        allocator(1024),
        seen(_vpackOptions) {}

  ~AggregatorSortedUnique() { reset(); }

  // cppcheck-suppress virtualCallInConstructor
  void reset() override final {
    seen.clear();
    allocator.clear();
    builder.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(reinterpret_cast<uint8_t const*>(pos));
  }

  AqlValue get() const override final {
    builder.openArray();
    for (auto const& it : seen) {
      builder.add(it);
    }

    // always close the Builder
    builder.close();
    return AqlValue(builder.slice());
  }

  MemoryBlockAllocator allocator;
  std::set<velocypack::Slice, basics::VelocyPackHelper::VPackLess<true>> seen;
  mutable arangodb::velocypack::Builder builder;
};

/// @brief the coordinator variant of SORTED_UNIQUE
struct AggregatorSortedUniqueStep2 final : public AggregatorSortedUnique {
  explicit AggregatorSortedUniqueStep2(velocypack::Options const* opts)
      : AggregatorSortedUnique(opts) {}

  void reduce(AqlValue const& cmpValue) override final {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (VPackSlice it : VPackArrayIterator(s)) {
      if (seen.find(it) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(reinterpret_cast<uint8_t const*>(pos));
    }
  }
};

struct AggregatorCountDistinct : public Aggregator {
  explicit AggregatorCountDistinct(velocypack::Options const* opts)
      : Aggregator(opts),
        allocator(1024),
        seen(512, basics::VelocyPackHelper::VPackHash(),
             basics::VelocyPackHelper::VPackEqual(_vpackOptions)) {}

  ~AggregatorCountDistinct() { reset(); }

  // cppcheck-suppress virtualCallInConstructor
  void reset() override final {
    seen.clear();
    allocator.clear();
  }

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (seen.find(s) != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(reinterpret_cast<uint8_t const*>(pos));
  }

  AqlValue get() const override final {
    uint64_t value = seen.size();
    return AqlValue(AqlValueHintUInt(value));
  }

  MemoryBlockAllocator allocator;
  std::unordered_set<velocypack::Slice, basics::VelocyPackHelper::VPackHash, basics::VelocyPackHelper::VPackEqual> seen;
};

/// @brief the coordinator variant of COUNT_DISTINCT
struct AggregatorCountDistinctStep2 final : public AggregatorCountDistinct {
  explicit AggregatorCountDistinctStep2(velocypack::Options const* opts)
      : AggregatorCountDistinct(opts) {}

  void reduce(AqlValue const& cmpValue) override {
    AqlValueMaterializer materializer(_vpackOptions);

    VPackSlice s = materializer.slice(cmpValue, true);

    if (!s.isArray()) {
      return;
    }

    for (VPackSlice it : VPackArrayIterator(s)) {
      if (seen.find(s) != seen.end()) {
        // already saw the same value
        return;
      }

      char* pos = allocator.store(it.startAs<char>(), it.byteSize());
      seen.emplace(reinterpret_cast<uint8_t const*>(pos));
    }
  }
};

/// @brief all available aggregators with their meta data
std::unordered_map<std::string, AggregatorInfo> const aggregators = {
    {"LENGTH",
     {
         [](velocypack::Options const* opts) {
           return std::make_unique<AggregatorLength>(opts);
         },
         doesNotRequireInput, official, "LENGTH",
         "SUM"  // we need to sum up the lengths from the DB servers
     }},
    {"MIN",
     {
         [](velocypack::Options const* opts) {
           return std::make_unique<AggregatorMin>(opts);
         },
         doesRequireInput, official, "MIN",
         "MIN"  // min is commutative
     }},
    {"MAX",
     {
         [](velocypack::Options const* opts) {
           return std::make_unique<AggregatorMax>(opts);
         },
         doesRequireInput, official, "MAX",
         "MAX"  // max is commutative
     }},
    {"SUM",
     {
         [](velocypack::Options const* opts) {
           return std::make_unique<AggregatorSum>(opts);
         },
         doesRequireInput, official, "SUM",
         "SUM"  // sum is commutative
     }},
    {"AVERAGE",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorAverage>(opts);
      },
      doesRequireInput, official, "AVERAGE_STEP1", "AVERAGE_STEP2"}},
    {"AVERAGE_STEP1",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorAverageStep1>(opts);
      },
      doesRequireInput, internalOnly, "", "AVERAGE_STEP1"}},
    {"AVERAGE_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorAverageStep2>(opts);
      },
      doesRequireInput, internalOnly, "", "AVERAGE_STEP2"}},
    {"VARIANCE_POPULATION",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVariance>(opts, true);
      },
      doesRequireInput, official, "VARIANCE_POPULATION_STEP1",
      "VARIANCE_POPULATION_STEP2"}},
    {"VARIANCE_POPULATION_STEP1",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep1>(opts, true);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_POPULATION_STEP1"}},
    {"VARIANCE_POPULATION_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep2>(opts, true);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_POPULATION_STEP2"}},
    {"VARIANCE_SAMPLE",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVariance>(opts, false);
      },
      doesRequireInput, official, "VARIANCE_SAMPLE_STEP1",
      "VARIANCE_SAMPLE_STEP2"}},
    {"VARIANCE_SAMPLE_STEP1",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep1>(opts, false);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_SAMPLE_STEP1"}},
    {"VARIANCE_SAMPLE_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep2>(opts, false);
      },
      doesRequireInput, internalOnly, "", "VARIANCE_SAMPLE_STEP2"}},
    {"STDDEV_POPULATION",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorStddev>(opts, true);
      },
      doesRequireInput, official, "STDDEV_POPULATION_STEP1",
      "STDDEV_POPULATION_STEP2"}},
    {"STDDEV_POPULATION_STEP1",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep1>(opts, true);
      },
      doesRequireInput, internalOnly, "", "STDDEV_POPULATION_STEP1"}},
    {"STDDEV_POPULATION_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorStddevBaseStep2>(opts, true);
      },
      doesRequireInput, internalOnly, "", "STDDEV_POPULATION_STEP2"}},
    {"STDDEV_SAMPLE",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorStddev>(opts, false);
      },
      doesRequireInput, official, "STDDEV_SAMPLE_STEP1", "STDDEV_SAMPLE_STEP2"}},
    {"STDDEV_SAMPLE_STEP1",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorVarianceBaseStep1>(opts, false);
      },
      doesRequireInput, internalOnly, "", "STDDEV_SAMPLE_STEP1"}},
    {"STDDEV_SAMPLE_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorStddevBaseStep2>(opts, false);
      },
      doesRequireInput, internalOnly, "", "STDDEV_SAMPLE_STEP2"}},
    {"UNIQUE",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorUnique>(opts);
      },
      doesRequireInput, official, "UNIQUE", "UNIQUE_STEP2"}},
    {"UNIQUE_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorUniqueStep2>(opts);
      },
      doesRequireInput, internalOnly, "", "UNIQUE_STEP2"}},
    {"SORTED_UNIQUE",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorSortedUnique>(opts);
      },
      doesRequireInput, official, "SORTED_UNIQUE", "SORTED_UNIQUE_STEP2"}},
    {"SORTED_UNIQUE_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorSortedUniqueStep2>(opts);
      },
      doesRequireInput, internalOnly, "", "SORTED_UNIQUE_STEP2"}},
    {"COUNT_DISTINCT",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorCountDistinct>(opts);
      },
      doesRequireInput, official, "UNIQUE", "COUNT_DISTINCT_STEP2"}},
    {"COUNT_DISTINCT_STEP2",
     {[](velocypack::Options const* opts) {
        return std::make_unique<AggregatorCountDistinctStep2>(opts);
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

std::unique_ptr<Aggregator> Aggregator::fromTypeString(velocypack::Options const* opts,
                                                       std::string const& type) {
  // will always return a valid generator function or throw an exception
  auto generator = Aggregator::factoryFromTypeString(type);
  TRI_ASSERT(generator != nullptr);

  return (*generator)(opts);
}

std::unique_ptr<Aggregator> Aggregator::fromVPack(velocypack::Options const* opts,
                                                  arangodb::velocypack::Slice const& slice,
                                                  char const* nameAttribute) {
  VPackSlice variable = slice.get(nameAttribute);

  if (variable.isString()) {
    return fromTypeString(opts, variable.copyString());
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

Aggregator::Factory Aggregator::factoryFromTypeString(
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
