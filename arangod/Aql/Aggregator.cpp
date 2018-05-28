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

std::unique_ptr<Aggregator> Aggregator::fromTypeString(transaction::Methods* trx,
                                                       std::string const& type) {
  if (type == "LENGTH" || type == "COUNT") {
    return std::make_unique<AggregatorLength>(trx);
  }
  if (type == "MIN") {
    return std::make_unique<AggregatorMin>(trx);
  }
  if (type == "MAX") {
    return std::make_unique<AggregatorMax>(trx);
  }
  if (type == "SUM") {
    return std::make_unique<AggregatorSum>(trx);
  }
  if (type == "AVERAGE" || type == "AVG") {
    return std::make_unique<AggregatorAverage>(trx);
  }
  if (type == "VARIANCE_POPULATION" || type == "VARIANCE") {
    return std::make_unique<AggregatorVariance>(trx, true);
  }
  if (type == "VARIANCE_SAMPLE") {
    return std::make_unique<AggregatorVariance>(trx, false);
  }
  if (type == "STDDEV_POPULATION" || type == "STDDEV") {
    return std::make_unique<AggregatorStddev>(trx, true);
  }
  if (type == "STDDEV_SAMPLE") {
    return std::make_unique<AggregatorStddev>(trx, false);
  }
  if (type == "UNIQUE") {
    return std::make_unique<AggregatorUnique>(trx, false);
  }
  if (type == "UNIQUE_ADD") {
    return std::make_unique<AggregatorUnique>(trx, true);
  }
  if (type == "SORTED_UNIQUE") {
    return std::make_unique<AggregatorSortedUnique>(trx, false);
  }
  if (type == "SORTED_UNIQUE_ADD") {
    return std::make_unique<AggregatorSortedUnique>(trx, true);
  }
  if (type == "COUNT_DISTINCT" || type == "COUNT_UNIQUE") {
    return std::make_unique<AggregatorCountDistinct>(trx, false);
  }
  if (type == "COUNT_UNIQUE_ADD") {
    return std::make_unique<AggregatorCountDistinct>(trx, true);
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

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid aggregate function");
}

bool Aggregator::isSupported(std::string const& type) {
  return (type == "LENGTH" || type == "COUNT" || type == "MIN" ||
          type == "MAX" || type == "SUM" || type == "AVERAGE" ||
          type == "AVG" || type == "VARIANCE_POPULATION" ||
          type == "VARIANCE" || type == "VARIANCE_SAMPLE" ||
          type == "STDDEV_POPULATION" || type == "STDDEV" ||
          type == "STDDEV_SAMPLE" || type == "UNIQUE" ||
          type == "SORTED_UNIQUE" ||
          type == "COUNT_DISTINCT" || type == "COUNT_UNIQUE");
}

bool Aggregator::requiresInput(std::string const& type) {
  if (type == "LENGTH" || type == "COUNT") {
    // LENGTH/COUNT do not require its input parameter, so
    // it can be optimized away
    return false;
  }
  // all other functions require their input
  return true;
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

AqlValue AggregatorSortedUnique::stealValue() {
  builder.openArray();
  for (auto const& it : seen) {
    builder.add(it);
  }
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

AqlValue AggregatorCountDistinct::stealValue() {
  if (seen.empty()) {
    return AqlValue(AqlValueHintUInt(0));
  }

  uint64_t value = seen.size();
  reset();
  return AqlValue(AqlValueHintUInt(value));
}
