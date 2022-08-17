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
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "VocBase/Methods/Tasks.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include <cstdint>
#include <optional>
#include <string>

namespace arangodb::pregel {

enum class MessageType {
  GraphLoaded,
  CleanupFinished,
  RecoveryFinished,
  GssFinished
};

struct Message {
  virtual auto type() const -> MessageType = 0;
  virtual ~Message(){};
};

// ------ events sent from worker to conductor -------

struct GraphLoaded : Message {
  std::string senderId;
  ExecutionNumber executionNumber;
  uint64_t vertexCount;
  uint64_t edgeCount;
  GraphLoaded(){};
  GraphLoaded(std::string const& senderId, ExecutionNumber executionNumber,
              uint64_t vertexCount, uint64_t edgeCount)
      : senderId{senderId},
        executionNumber{executionNumber},
        vertexCount{vertexCount},
        edgeCount{edgeCount} {}
  auto type() const -> MessageType override { return MessageType::GraphLoaded; }
};

template<typename Inspector>
auto inspect(Inspector& f, GraphLoaded& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount));
}

struct GssFinished : Message {
  std::string senderId;
  ExecutionNumber executionNumber;
  uint64_t gss;
  VPackBuilder reports;
  VPackBuilder messageStats;
  VPackBuilder aggregators;
  GssFinished(){};
  GssFinished(std::string const& senderId, ExecutionNumber executionNumber,
              uint64_t gss, VPackBuilder reports, VPackBuilder messageStats,
              VPackBuilder aggregators)
      : senderId{senderId},
        executionNumber{executionNumber},
        gss{gss},
        reports{std::move(reports)},
        messageStats{std::move(messageStats)},
        aggregators{std::move(aggregators)} {}
  auto type() const -> MessageType override { return MessageType::GssFinished; }
};

template<typename Inspector>
auto inspect(Inspector& f, GssFinished& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss), f.field("reports", x.reports),
      f.field("messageStats", x.messageStats),
      f.field("aggregators", x.aggregators));
}

struct CleanupFinished : Message {
  std::string senderId;
  ExecutionNumber executionNumber;
  VPackBuilder reports;
  CleanupFinished(){};
  CleanupFinished(std::string const& senderId, ExecutionNumber executionNumber,
                  VPackBuilder reports)
      : senderId{senderId},
        executionNumber{executionNumber},
        reports{std::move(reports)} {}
  auto type() const -> MessageType override {
    return MessageType::CleanupFinished;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, CleanupFinished& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("reports", x.reports));
}

struct RecoveryFinished : Message {
  std::string senderId;
  ExecutionNumber executionNumber;
  uint64_t gss;
  VPackBuilder aggregators;
  RecoveryFinished(){};
  RecoveryFinished(std::string const& senderId, ExecutionNumber executionNumber,
                   uint64_t gss, VPackBuilder aggregators)
      : senderId{senderId},
        executionNumber{executionNumber},
        gss{gss},
        aggregators{std::move(aggregators)} {}
  auto type() const -> MessageType override {
    return MessageType::RecoveryFinished;
  }
};

template<typename Inspector>
auto inspect(Inspector& f, RecoveryFinished& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("aggregators", x.aggregators));
}

struct StatusUpdated {
  std::string senderId;
  ExecutionNumber executionNumer;
  Status status;
};

template<typename Inspector>
auto inspect(Inspector& f, StatusUpdated& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumer),
      f.field("status", x.status));
}

// worker -> conductor as immediate answer

struct GssPrepared {
  std::string senderId;
  uint64_t activeCount;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder messages;
  VPackBuilder aggregators;
};
template<typename Inspector>
auto inspect(Inspector& f, GssPrepared& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field("activeCount", x.activeCount),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("messages", x.messages), f.field("aggregators", x.aggregators));
}

struct PregelResults {
  VPackBuilder results;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelResults& x) {
  return f.object(x).fields(f.field("results", x.results));
}

struct GssStarted {};
template<typename Inspector>
auto inspect(Inspector& f, GssStarted& x) {
  return f.object(x).fields();
}

struct CleanupStarted {};
template<typename Inspector>
auto inspect(Inspector& f, CleanupStarted& x) {
  return f.object(x).fields();
}

struct GssCanceled {};
template<typename Inspector>
auto inspect(Inspector& f, GssCanceled& x) {
  return f.object(x).fields();
}

struct RecoveryFinalized {};
template<typename Inspector>
auto inspect(Inspector& f, RecoveryFinalized& x) {
  return f.object(x).fields();
}

struct RecoveryContinued {};
template<typename Inspector>
auto inspect(Inspector& f, RecoveryContinued& x) {
  return f.object(x).fields();
}

// ------ commands sent from conductor to worker -------

struct PrepareGss {
  ExecutionNumber executionNumber;
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

struct StartGss {
  ExecutionNumber executionNumber;
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
  bool activateAll;
  VPackBuilder toWorkerMessages;
  VPackBuilder aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, StartGss& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("activateAll", x.activateAll),
      f.field("masterToWorkerMessages", x.toWorkerMessages),
      f.field("aggregators", x.aggregators));
}

struct CancelGss {
  ExecutionNumber executionNumber;
  uint64_t gss;
};

template<typename Inspector>
auto inspect(Inspector& f, CancelGss& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss));
}

struct StartCleanup {
  ExecutionNumber executionNumber;
  uint64_t gss;
  bool withStoring;
};

template<typename Inspector>
auto inspect(Inspector& f, StartCleanup& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("withStoring", x.withStoring));
}

struct ContinueRecovery {
  ExecutionNumber executionNumber;
  VPackBuilder aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, ContinueRecovery& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("aggregators", x.aggregators));
}

struct FinalizeRecovery {
  ExecutionNumber executionNumber;
  uint64_t gss;
};

template<typename Inspector>
auto inspect(Inspector& f, FinalizeRecovery& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field(Utils::globalSuperstepKey, x.gss));
}

struct CollectPregelResults {
  ExecutionNumber executionNumber;
  bool withId;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectPregelResults& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("withId", x.withId).fallback(false));
}
}  // namespace arangodb::pregel
