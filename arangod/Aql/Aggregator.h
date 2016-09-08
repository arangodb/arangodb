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
#include "Aql/AqlValue.h"

#include <velocypack/Builder.h>

namespace arangodb {
class Transaction;

namespace aql {

struct Aggregator {
  Aggregator() = delete;
  Aggregator(Aggregator const&) = delete;
  Aggregator& operator=(Aggregator const&) = delete;

  explicit Aggregator(arangodb::Transaction* trx) : trx(trx) {}
  virtual ~Aggregator() = default;
  virtual char const* name() const = 0;
  virtual void reset() = 0;
  virtual void reduce(AqlValue const&) = 0;
  virtual AqlValue stealValue() = 0;

  static Aggregator* fromTypeString(arangodb::Transaction*,
                                    std::string const&);
  static Aggregator* fromVPack(arangodb::Transaction*,
                               arangodb::velocypack::Slice const&, char const*);

  static bool isSupported(std::string const&);
  static bool requiresInput(std::string const&);

  arangodb::Transaction* trx;

  arangodb::velocypack::Builder builder;
};

struct AggregatorLength final : public Aggregator {
  explicit AggregatorLength(arangodb::Transaction* trx)
      : Aggregator(trx), count(0) {}
  AggregatorLength(arangodb::Transaction* trx, uint64_t initialCount)
      : Aggregator(trx), count(initialCount) {}

  char const* name() const override final { return "LENGTH"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  uint64_t count;
};

struct AggregatorMin final : public Aggregator {
  explicit AggregatorMin(arangodb::Transaction* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMin();

  char const* name() const override final { return "MIN"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  AqlValue value;
};

struct AggregatorMax final : public Aggregator {
  explicit AggregatorMax(arangodb::Transaction* trx)
      : Aggregator(trx), value() {}

  ~AggregatorMax();

  char const* name() const override final { return "MAX"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  AqlValue value;
};

struct AggregatorSum final : public Aggregator {
  explicit AggregatorSum(arangodb::Transaction* trx)
      : Aggregator(trx), sum(0.0), invalid(false) {}

  char const* name() const override final { return "SUM"; }

  void reset() override final;
  void reduce(AqlValue const&) override final;
  AqlValue stealValue() override final;

  double sum;
  bool invalid;
};

struct AggregatorAverage final : public Aggregator {
  explicit AggregatorAverage(arangodb::Transaction* trx)
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
  AggregatorVarianceBase(arangodb::Transaction* trx, bool population)
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
  AggregatorVariance(arangodb::Transaction* trx, bool population)
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
  AggregatorStddev(arangodb::Transaction* trx, bool population)
      : AggregatorVarianceBase(trx, population) {}

  char const* name() const override final {
    if (population) {
      return "STDDEV_POPULATION";
    }
    return "STDDEV_SAMPLE";
  }

  AqlValue stealValue() override final;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
