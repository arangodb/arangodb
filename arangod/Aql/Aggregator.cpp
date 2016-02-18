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

using namespace arangodb::basics;
using namespace arangodb::aql;

Aggregator* Aggregator::fromTypeString(arangodb::AqlTransaction* trx,
                                       std::string const& type) {
  if (type == "LENGTH" || type == "COUNT") {
    return new AggregatorLength(trx);
  }
  if (type == "MIN") {
    return new AggregatorMin(trx);
  }
  if (type == "MAX") {
    return new AggregatorMax(trx);
  }
  if (type == "SUM") {
    return new AggregatorSum(trx);
  }
  if (type == "AVERAGE" || type == "AVG") {
    return new AggregatorAverage(trx);
  }
  if (type == "VARIANCE_POPULATION" || type == "VARIANCE") {
    return new AggregatorVariance(trx, true);
  }
  if (type == "VARIANCE_SAMPLE") {
    return new AggregatorVariance(trx, false);
  }
  if (type == "STDDEV_POPULATION" || type == "STDDEV") {
    return new AggregatorStddev(trx, true);
  }
  if (type == "STDDEV_SAMPLE") {
    return new AggregatorStddev(trx, false);
  }

  // aggregator function name should have been validated before
  TRI_ASSERT(false);
  return nullptr;
}

Aggregator* Aggregator::fromJson(arangodb::AqlTransaction* trx,
                                 arangodb::basics::Json const& json,
                                 char const* variableName) {
  arangodb::basics::Json variableJson = json.get(variableName);

  if (variableJson.isString()) {
    std::string const type(variableJson.json()->_value._string.data,
                           variableJson.json()->_value._string.length - 1);
    return fromTypeString(trx, type);
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
          type == "STDDEV_SAMPLE");
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

void AggregatorLength::reduce(AqlValue const&,
                              TRI_document_collection_t const*) {
  ++count;
}

AqlValue AggregatorLength::stealValue() {
  uint64_t copy = count;
  count = 0;
  return AqlValue(new Json(static_cast<double>(copy)));
}

AggregatorMin::~AggregatorMin() { value.destroy(); }

void AggregatorMin::reset() { value.destroy(); }

void AggregatorMin::reduce(AqlValue const& cmpValue,
                           TRI_document_collection_t const* cmpColl) {
  if (value.isEmpty() ||
      (!cmpValue.isNull(true) &&
       AqlValue::Compare(trx, value, coll, cmpValue, cmpColl, true) > 0)) {
    // the value `null` itself will not be used in MIN() to compare lower than
    // e.g. value `false`
    value.destroy();
    value = cmpValue.clone();
    coll = cmpColl;
  }
}

AqlValue AggregatorMin::stealValue() {
  if (value.isEmpty()) {
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }
  AqlValue copy = value;
  value.erase();
  return copy;
}

AggregatorMax::~AggregatorMax() { value.destroy(); }

void AggregatorMax::reset() { value.destroy(); }

void AggregatorMax::reduce(AqlValue const& cmpValue,
                           TRI_document_collection_t const* cmpColl) {
  if (value.isEmpty() ||
      AqlValue::Compare(trx, value, coll, cmpValue, cmpColl, true) < 0) {
    value.destroy();
    value = cmpValue.clone();
    coll = cmpColl;
  }
}

AqlValue AggregatorMax::stealValue() {
  if (value.isEmpty()) {
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }
  AqlValue copy = value;
  value.erase();
  return copy;
}

void AggregatorSum::reset() {
  sum = 0.0;
  invalid = false;
}

void AggregatorSum::reduce(AqlValue const& cmpValue,
                           TRI_document_collection_t const*) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      bool failed = false;
      double const number = cmpValue.toNumber(failed);
      if (!failed && !std::isnan(number) && number != HUGE_VAL &&
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
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }

  return AqlValue(new arangodb::basics::Json(sum));
}

void AggregatorAverage::reset() {
  count = 0;
  sum = 0.0;
  invalid = false;
}

void AggregatorAverage::reduce(AqlValue const& cmpValue,
                               TRI_document_collection_t const*) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      bool failed = false;
      double const number = cmpValue.toNumber(failed);
      if (!failed && !std::isnan(number) && number != HUGE_VAL &&
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
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }

  TRI_ASSERT(count > 0);

  return AqlValue(new arangodb::basics::Json(sum / static_cast<double>(count)));
}

void AggregatorVarianceBase::reset() {
  count = 0;
  sum = 0.0;
  mean = 0.0;
  invalid = false;
}

void AggregatorVarianceBase::reduce(AqlValue const& cmpValue,
                                    TRI_document_collection_t const*) {
  if (!invalid) {
    if (cmpValue.isNull(true)) {
      // ignore `null` values here
      return;
    }
    if (cmpValue.isNumber()) {
      bool failed = false;
      double const number = cmpValue.toNumber(failed);
      if (!failed && !std::isnan(number) && number != HUGE_VAL &&
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
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }

  TRI_ASSERT(count > 0);

  if (!population) {
    TRI_ASSERT(count > 1);
    return AqlValue(
        new arangodb::basics::Json(sum / static_cast<double>(count - 1)));
  }
  return AqlValue(new arangodb::basics::Json(sum / static_cast<double>(count)));
}

AqlValue AggregatorStddev::stealValue() {
  if (invalid || count == 0 || (count == 1 && !population) || std::isnan(sum) ||
      sum == HUGE_VAL || sum == -HUGE_VAL) {
    return AqlValue(new arangodb::basics::Json(arangodb::basics::Json::Null));
  }

  TRI_ASSERT(count > 0);

  if (!population) {
    TRI_ASSERT(count > 1);
    return AqlValue(
        new arangodb::basics::Json(sqrt(sum / static_cast<double>(count - 1))));
  }
  return AqlValue(
      new arangodb::basics::Json(sqrt(sum / static_cast<double>(count))));
}
