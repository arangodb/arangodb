////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pregel/Aggregator.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "velocypack/Slice.h"
#include <cstdint>
#include <optional>
#include <string>

namespace arangodb::pregel {

struct AggregatorWrapper {
  std::shared_ptr<AggregatorHandler> aggregators = nullptr;
};

template<typename Inspector>
auto inspect(Inspector& f, AggregatorWrapper& x) {
  if constexpr (Inspector::isLoading) {
    return arangodb::inspection::Status{};
  } else {
    x.aggregators->serializeValues(f.builder());
    return arangodb::inspection::Status{};
  }
}

// ------ events sent from worker to conductor -------

struct GraphLoaded {
  std::string senderId;
  uint64_t executionNumber;
  uint64_t vertexCount;
  uint64_t edgeCount;
};

template<typename Inspector>
auto inspect(Inspector& f, GraphLoaded& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount));
}

struct RecoveryFinished {
  std::string senderId;
  uint64_t executionNumber;
  uint64_t gss;
  AggregatorWrapper aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, RecoveryFinished& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field(Utils::aggregatorValuesKey, x.aggregators));
}

struct StatusUpdated {
  std::string senderId;
  uint64_t executionNumer;
  Status status;
};

template<typename Inspector>
auto inspect(Inspector& f, StatusUpdated& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumer),
      f.field("status", x.status));
}

// ------ commands sent from conductor to worker -------

struct PrepareGss {
  uint64_t executionNumber;
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
};

template<typename Inspector>
auto inspect(Inspector& f, PrepareGss& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount));
}

struct CancelGss {
  uint64_t executionNumber;
  uint64_t gss;
};

template<typename Inspector>
auto inspect(Inspector& f, CancelGss& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss));
}

struct FinalizeExecution {
  uint64_t executionNumber;
  uint64_t gss;
  bool withStoring;
};

template<typename Inspector>
auto inspect(Inspector& f, FinalizeExecution& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("withStoring", x.withStoring));
}

struct ContinueRecovery {
  uint64_t executionNumber;
  AggregatorWrapper aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, ContinueRecovery& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::aggregatorValuesKey, x.aggregators));
}

struct FinalizeRecovery {
  uint64_t executionNumber;
  uint64_t gss;
};

template<typename Inspector>
auto inspect(Inspector& f, FinalizeRecovery& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss));
}

struct CollectPregelResults {
  uint64_t executionNumber;
  bool withId;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectPregelResults& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("withId", x.withId).fallback(false));
}
}  // namespace arangodb::pregel
