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
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {

constexpr bool doesRequireInput = true;
constexpr bool doesNotRequireInput = false;

constexpr bool official = true;
constexpr bool internalOnly = false;

struct AggregatorInfo {
  std::function<std::unique_ptr<Aggregator>(transaction::Methods* trx)> generator;
  bool requiresInput;
  bool isVisible;
  std::string pushToDBServerAs;
  std::string runOnCoordinatorAs;
};

std::unordered_map<std::string, AggregatorInfo> const aggregators = {
  // LENGTH and COUNT are identical. both do not require any input
  { "LENGTH", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorLength>(trx); }, 
    doesNotRequireInput, official,
    "LENGTH",
    "SUM" // we need to sum up the lengths from the DB servers 
  } }, 
  { "MIN",{ 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorMin>(trx); }, 
    doesRequireInput, official,
    "MIN",
    "MIN" // min is commutative 
  } }, 
  { "MAX", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorMax>(trx); }, 
    doesRequireInput, official,
    "MAX",
    "MAX" // max is commutative 
  } }, 
  { "SUM", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorSum>(trx); }, 
    doesRequireInput, official,
    "SUM",
    "SUM" // sum is commutative 
  } }, 
  // AVERAGE and AVG are identical
  { "AVERAGE", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorAverage>(trx); }, 
    doesRequireInput, official,
    "AVERAGE_STEP1",
    "AVERAGE_STEP2"
  } }, 
  { "AVERAGE_STEP1", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorAverageStep1>(trx); }, 
    doesRequireInput, internalOnly,
    "",
    "AVERAGE_STEP1"
  } }, 
  { "AVERAGE_STEP2", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorAverageStep2>(trx); }, 
    doesRequireInput, internalOnly,
    "",
    "AVERAGE_STEP2"
  } }, 
  { "VARIANCE_POPULATION", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorVariance>(trx, true); }, 
    doesRequireInput, official,
    "",
    "VARIANCE_POPULATION"
  } }, 
  { "VARIANCE_SAMPLE", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorVariance>(trx, false); }, 
    doesRequireInput, official,
    "",
    "VARIANCE_SAMPLE" 
  } }, 
  { "STDDEV_POPULATION", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorStddev>(trx, true); }, 
    doesRequireInput, official,
    "",
    "STDDEV_POPULATION"
  } }, 
  { "STDDEV_SAMPLE", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorStddev>(trx, false); }, 
    doesRequireInput, official,
    "",
    "STDDEV_SAMPLE" 
  } }, 
  { "UNIQUE", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorUnique>(trx, false); }, 
    doesRequireInput, official,
    "UNIQUE",
    "UNIQUE_STEP2" 
  } }, 
  { "UNIQUE_STEP2", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorUnique>(trx, true); }, 
    doesRequireInput, internalOnly,
    "",
    "UNIQUE_STEP2"
  } }, 
  { "SORTED_UNIQUE", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorSortedUnique>(trx, false); }, 
    doesRequireInput, official,
    "SORTED_UNIQUE",
    "SORTED_UNIQUE_STEP2"
  } }, 
  { "SORTED_UNIQUE_STEP2", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorSortedUnique>(trx, true); }, 
    doesRequireInput, internalOnly,
    "",
    "SORTED_UNIQUE_STEP2"
  } }, 
  // COUNT_UNIQUE and COUNT_DISTINCT are identical
  { "COUNT_DISTINCT", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorCountDistinct>(trx, false); }, 
    doesRequireInput, official,
    "UNIQUE",
    "COUNT_DISTINCT_STEP2"
  } }, 
  { "COUNT_DISTINCT_STEP2", { 
    [](transaction::Methods* trx) { return std::make_unique<AggregatorCountDistinct>(trx, true); }, 
    doesRequireInput, internalOnly,
    "",
    "COUNT_DISTINCT_STEP2"
  } } 
};

std::unordered_map<std::string, std::string> const aliases = {
  { "COUNT", "LENGTH" },
  { "AVG", "AVERAGE" },
  { "VARIANCE", "VARIANCE_POPULATION" },
  { "STDDEV", "STDDEV_POPULATION" },
  { "COUNT_UNIQUE", "COUNT_DISTINCT" }
};

} // namespace

std::unique_ptr<Aggregator> Aggregator::fromTypeString(transaction::Methods* trx,
                                                       std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));

  if (it != ::aggregators.end()) { 
    return (*it).second.generator(trx);
  }
  // aggregator function name should have been validated before
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

std::unique_ptr<Aggregator> Aggregator::fromVPack(transaction::Methods* trx,
                                                  arangodb::velocypack::Slice const& slice,
                                                  char const* variableName) {
  VPackSlice variable = slice.get(variableName);

  if (variable.isString()) {
    return fromTypeString(trx, variable.copyString());
  }
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
  return (*it).second.isVisible;
}

bool Aggregator::requiresInput(std::string const& type) {
  auto it = ::aggregators.find(translateAlias(type));
  
  if (it != ::aggregators.end()) {
    return (*it).second.requiresInput;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid aggregator type");
}

void AggregatorLength::reset() { count = 0; }

void AggregatorLength::reduce(AqlValue const&) {
  ++count;
}

AqlValue AggregatorLength::stealValue() {
  uint64_t value = count;
  reset();
  return AqlValue(AqlValueHintUInt(value));
}

AggregatorMin::~AggregatorMin() { value.destroy(); }

void AggregatorMin::reset() { value.erase(); }

void AggregatorMin::reduce(AqlValue const& cmpValue) {
  if (value.isEmpty() ||
      (!cmpValue.isNull(true) &&
       AqlValue::Compare(trx, value, cmpValue, true) > 0)) {
    // the value `null` itself will not be used in MIN() to compare lower than
    // e.g. value `false`
    value.destroy();
    value = cmpValue.clone();
  }
}

AqlValue AggregatorMin::stealValue() {
  if (value.isEmpty()) {
    return AqlValue(AqlValueHintNull());
  }
  AqlValue copy = value;
  value.erase();
  return copy;
}

AggregatorMax::~AggregatorMax() { value.destroy(); }

void AggregatorMax::reset() { value.erase(); }

void AggregatorMax::reduce(AqlValue const& cmpValue) {
  if (value.isEmpty() ||
      AqlValue::Compare(trx, value, cmpValue, true) < 0) {
    value.destroy();
    value = cmpValue.clone();
  }
}

AqlValue AggregatorMax::stealValue() {
  if (value.isEmpty()) {
    return AqlValue(AqlValueHintNull());
  }
  AqlValue copy = value;
  value.erase();
  return copy;
}

void AggregatorSum::reset() {
  sum = 0.0;
  invalid = false;
}

void AggregatorSum::reduce(AqlValue const& cmpValue) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      double const number = cmpValue.toDouble(trx);
      if (!std::isnan(number) && number != HUGE_VAL &&
          number != -HUGE_VAL) {
        sum += number;
        return;
      }
    }
  }

  invalid = true;
}

AqlValue AggregatorSum::stealValue() {
  if (invalid || std::isnan(sum) || sum == HUGE_VAL || sum == -HUGE_VAL) {
    return AqlValue(AqlValueHintNull());
  }

  builder.clear();
  builder.add(VPackValue(sum));
  AqlValue temp(builder.slice());
  reset();
  return temp;
}

void AggregatorAverage::reset() {
  count = 0;
  sum = 0.0;
  invalid = false;
}

void AggregatorAverage::reduce(AqlValue const& cmpValue) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      double const number = cmpValue.toDouble(trx);
      if (!std::isnan(number) && number != HUGE_VAL &&
          number != -HUGE_VAL) {
        sum += number;
        ++count;
        return;
      }
    }
  }

  invalid = true;
}

AqlValue AggregatorAverage::stealValue() {
  if (invalid || count == 0 || std::isnan(sum) || sum == HUGE_VAL ||
      sum == -HUGE_VAL) {
    return AqlValue(AqlValueHintNull());
  }

  TRI_ASSERT(count > 0);
  
  builder.clear();
  builder.add(VPackValue(sum / count));
  AqlValue temp(builder.slice());
  reset();
  return temp;
}

AqlValue AggregatorAverageStep1::stealValue() {
  builder.clear();
  builder.openArray();
  if (invalid || count == 0 || std::isnan(sum) || sum == HUGE_VAL ||
      sum == -HUGE_VAL) {
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
  
void AggregatorAverageStep2::reduce(AqlValue const& cmpValue) {
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

void AggregatorVarianceBase::reset() {
  count = 0;
  sum = 0.0;
  mean = 0.0;
  invalid = false;
}

void AggregatorVarianceBase::reduce(AqlValue const& cmpValue) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      double const number = cmpValue.toDouble(trx);
      if (!std::isnan(number) && number != HUGE_VAL &&
          number != -HUGE_VAL) {
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

AqlValue AggregatorVariance::stealValue() {
  if (invalid || count == 0 || (count == 1 && !population) || std::isnan(sum) ||
      sum == HUGE_VAL || sum == -HUGE_VAL) {
    return AqlValue(AqlValueHintNull());
  }

  TRI_ASSERT(count > 0);
  
  builder.clear();
  if (!population) {
    TRI_ASSERT(count > 1);
    builder.add(VPackValue(sum / (count - 1)));
  }
  else {
    builder.add(VPackValue(sum / count));
  }
  
  AqlValue temp(builder.slice());
  reset();
  return temp;
}

AqlValue AggregatorStddev::stealValue() {
  if (invalid || count == 0 || (count == 1 && !population) || std::isnan(sum) ||
      sum == HUGE_VAL || sum == -HUGE_VAL) {
    return AqlValue(AqlValueHintNull());
  }

  TRI_ASSERT(count > 0);
    
  builder.clear();
  if (!population) {
    TRI_ASSERT(count > 1);
    builder.add(VPackValue(sqrt(sum / (count - 1))));
  }
  else {
    builder.add(VPackValue(sqrt(sum / count)));
  }

  AqlValue temp(builder.slice());
  reset();
  return temp;
}
      
AggregatorUnique::AggregatorUnique(transaction::Methods* trx, bool isArrayInput)
    : Aggregator(trx), 
      allocator(1024), 
      seen(512, basics::VelocyPackHelper::VPackHash(), basics::VelocyPackHelper::VPackEqual(trx->transactionContext()->getVPackOptions())),
      isArrayInput(isArrayInput) {}
      
AggregatorUnique::~AggregatorUnique() { reset(); } 

void AggregatorUnique::reset() {
  seen.clear();
  builder.clear();
  allocator.clear();
}

void AggregatorUnique::reduce(AqlValue const& cmpValue) {
  AqlValueMaterializer materializer(trx);

  VPackSlice s = materializer.slice(cmpValue, true);
  
  auto adder = [this](VPackSlice s) {
    auto it = seen.find(s);
    
    if (it != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);

    if (builder.isClosed()) {
      builder.openArray();
    }
    builder.add(VPackSlice(pos));
  };

  if (isArrayInput) {
    if (!s.isArray()) {
      return;
    }
    for (auto it : VPackArrayIterator(s)) {
      adder(it);
    }
  } else {
    adder(s);
  }
}

AqlValue AggregatorUnique::stealValue() {
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

AggregatorSortedUnique::AggregatorSortedUnique(transaction::Methods* trx, bool isArrayInput)
    : Aggregator(trx), 
      allocator(1024),
      seen(trx->transactionContext()->getVPackOptions()),
      isArrayInput(isArrayInput) {}

AggregatorSortedUnique::~AggregatorSortedUnique() { reset(); } 

void AggregatorSortedUnique::reset() {
  seen.clear();
  allocator.clear();
  builder.clear();
}

void AggregatorSortedUnique::reduce(AqlValue const& cmpValue) {
  AqlValueMaterializer materializer(trx);

  VPackSlice s = materializer.slice(cmpValue, true);
  
  auto adder = [this](VPackSlice s) {
    auto it = seen.find(s);
    
    if (it != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);
  };
  
  if (isArrayInput) {
    if (!s.isArray()) {
      return;
    }
    for (auto it : VPackArrayIterator(s)) {
      adder(it);
    }
  } else {
    adder(s);
  }
}

AqlValue AggregatorSortedUnique::stealValue() {
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

AggregatorCountDistinct::AggregatorCountDistinct(transaction::Methods* trx, bool isArrayInput)
    : Aggregator(trx), 
      allocator(1024),
      seen(512, basics::VelocyPackHelper::VPackHash(), basics::VelocyPackHelper::VPackEqual(trx->transactionContext()->getVPackOptions())),
      isArrayInput(isArrayInput) {}

AggregatorCountDistinct::~AggregatorCountDistinct() { reset(); } 

void AggregatorCountDistinct::reset() {
  seen.clear();
  allocator.clear();
  builder.clear();
}

void AggregatorCountDistinct::reduce(AqlValue const& cmpValue) {
  AqlValueMaterializer materializer(trx);

  VPackSlice s = materializer.slice(cmpValue, true);

  auto adder = [this](VPackSlice s) {
    auto it = seen.find(s);
    
    if (it != seen.end()) {
      // already saw the same value
      return;
    }

    char* pos = allocator.store(s.startAs<char>(), s.byteSize());
    seen.emplace(pos);
  };
  
  if (isArrayInput) {
    if (!s.isArray()) {
      return;
    }
    for (auto it : VPackArrayIterator(s)) {
      adder(it);
    }
  } else {
    adder(s);
  }
}

AqlValue AggregatorCountDistinct::stealValue() {
  uint64_t value = seen.size();
  reset();
  return AqlValue(AqlValueHintUInt(value));
}
