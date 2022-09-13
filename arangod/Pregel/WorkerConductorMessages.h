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
#include "Pregel/Graph.h"
#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include "VocBase/Methods/Tasks.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace arangodb::pregel {

enum class MessageType { GraphLoaded, CleanupFinished, GssFinished };

struct Message {
  virtual auto type() const -> MessageType = 0;
  virtual ~Message(){};
};

// ------ events sent from worker to conductor -------

struct GraphLoaded : Message {
  uint64_t vertexCount;
  uint64_t edgeCount;
  GraphLoaded() noexcept = default;
  GraphLoaded(uint64_t vertexCount, uint64_t edgeCount)
      : vertexCount{vertexCount}, edgeCount{edgeCount} {}
  auto type() const -> MessageType override { return MessageType::GraphLoaded; }
};

template<typename Inspector>
auto inspect(Inspector& f, GraphLoaded& x) {
  return f.object(x).fields(f.field("vertexCount", x.vertexCount),
                            f.field("edgeCount", x.edgeCount));
}

struct GlobalSuperStepPrepared {
  std::string senderId;
  uint64_t activeCount;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder messages;
  VPackBuilder aggregators;
  GlobalSuperStepPrepared() noexcept = default;
  GlobalSuperStepPrepared(std::string senderId, uint64_t activeCount,
                          uint64_t vertexCount, uint64_t edgeCount,
                          VPackBuilder messages, VPackBuilder aggregators)
      : senderId{std::move(senderId)},
        activeCount{activeCount},
        vertexCount{vertexCount},
        edgeCount{edgeCount},
        messages{std::move(messages)},
        aggregators{std::move(aggregators)} {}
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepPrepared& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field("activeCount", x.activeCount),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("messages", x.messages), f.field("aggregators", x.aggregators));
}

struct GlobalSuperStepFinished : Message {
  std::string senderId;
  uint64_t gss;
  VPackBuilder reports;
  VPackBuilder messageStats;
  VPackBuilder aggregators;
  GlobalSuperStepFinished() noexcept {};
  GlobalSuperStepFinished(std::string senderId, uint64_t gss,
                          VPackBuilder reports, VPackBuilder messageStats,
                          VPackBuilder aggregators)
      : senderId{std::move(senderId)},
        gss{gss},
        reports{std::move(reports)},
        messageStats{std::move(messageStats)},
        aggregators{std::move(aggregators)} {}
  auto type() const -> MessageType override { return MessageType::GssFinished; }
};

template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepFinished& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field(Utils::globalSuperstepKey, x.gss),
                            f.field("reports", x.reports),
                            f.field("messageStats", x.messageStats),
                            f.field("aggregators", x.aggregators));
}

struct Stored {};
template<typename Inspector>
auto inspect(Inspector& f, Stored& x) {
  return f.object(x).fields();
}

struct CleanupFinished : Message {
  std::string senderId;
  VPackBuilder reports;
  CleanupFinished(){};
  CleanupFinished(std::string const& senderId, VPackBuilder reports)
      : senderId{senderId}, reports{std::move(reports)} {}
  auto type() const -> MessageType override {
    return MessageType::CleanupFinished;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, CleanupFinished& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field("reports", x.reports));
}

struct StatusUpdated {
  std::string senderId;
  Status status;
};

template<typename Inspector>
auto inspect(Inspector& f, StatusUpdated& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field("status", x.status));
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

// ------ commands sent from conductor to worker -------

struct LoadGraph {
  VPackBuilder details;
};
template<typename Inspector>
auto inspect(Inspector& f, LoadGraph& x) {
  return f.object(x).fields(f.field("details", x.details));
}

struct PrepareGlobalSuperStep {
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
};

template<typename Inspector>
auto inspect(Inspector& f, PrepareGlobalSuperStep& x) {
  return f.object(x).fields(f.field(Utils::globalSuperstepKey, x.gss),
                            f.field("vertexCount", x.vertexCount),
                            f.field("edgeCount", x.edgeCount));
}

struct RunGlobalSuperStep {
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
  bool activateAll;
  VPackBuilder toWorkerMessages;
  VPackBuilder aggregators;
};

template<typename Inspector>
auto inspect(Inspector& f, RunGlobalSuperStep& x) {
  return f.object(x).fields(
      f.field(Utils::globalSuperstepKey, x.gss),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("activateAll", x.activateAll),
      f.field("masterToWorkerMessages", x.toWorkerMessages),
      f.field("aggregators", x.aggregators));
}

struct Store {};
template<typename Inspector>
auto inspect(Inspector& f, Store& x) {
  return f.object(x).fields();
}

struct StartCleanup {
  uint64_t gss;
  bool withStoring;
};

template<typename Inspector>
auto inspect(Inspector& f, StartCleanup& x) {
  return f.object(x).fields(f.field(Utils::globalSuperstepKey, x.gss),
                            f.field("withStoring", x.withStoring));
}

struct CollectPregelResults {
  bool withId;
};

template<typename Inspector>
auto inspect(Inspector& f, CollectPregelResults& x) {
  return f.object(x).fields(f.field("withId", x.withId).fallback(false));
}

// or PregelShardMessage
struct PregelMessage {
  std::string senderId;
  uint64_t gss;
  PregelShard shard;
  VPackBuilder messages;
};

template<typename Inspector>
auto inspect(Inspector& f, PregelMessage& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field(Utils::globalSuperstepKey, x.gss),
                            f.field("shard", x.shard),
                            f.field("messages", x.messages));
}

// ---------------------- modern message ----------------------

using MessagePayload =
    std::variant<LoadGraph, ResultT<GraphLoaded>, PrepareGlobalSuperStep,
                 ResultT<GlobalSuperStepPrepared>, RunGlobalSuperStep,
                 ResultT<GlobalSuperStepFinished>, Store, ResultT<Stored>,
                 CollectPregelResults, PregelResults, StartCleanup,
                 CleanupStarted, StatusUpdated, CleanupFinished, PregelMessage>;

struct MessagePayloadSerializer : MessagePayload {};
template<class Inspector>
auto inspect(Inspector& f, MessagePayloadSerializer& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<LoadGraph>("loadGraph"),
      arangodb::inspection::type<ResultT<GraphLoaded>>("graphLoaded"),
      arangodb::inspection::type<PrepareGlobalSuperStep>(
          "prepareGlobalSuperStep"),
      arangodb::inspection::type<ResultT<GlobalSuperStepPrepared>>(
          "globalSuperStepPrepared"),
      arangodb::inspection::type<RunGlobalSuperStep>("runGlobalSuperStep"),
      arangodb::inspection::type<ResultT<GlobalSuperStepFinished>>(
          "globalSuperStepFinished"),
      arangodb::inspection::type<Store>("store"),
      arangodb::inspection::type<ResultT<Stored>>("stored"),
      arangodb::inspection::type<CollectPregelResults>("collectPregelResults"),
      arangodb::inspection::type<PregelResults>("pregelResults"),
      arangodb::inspection::type<StartCleanup>("startCleanup"),
      arangodb::inspection::type<CleanupStarted>("cleanupStarted"),
      arangodb::inspection::type<CleanupFinished>("cleanupFinished"),
      arangodb::inspection::type<PregelMessage>("pregelMessage"),
      arangodb::inspection::type<StatusUpdated>("statusUpdated"));
}
template<typename Inspector>
auto inspect(Inspector& f, MessagePayload& x) {
  if constexpr (Inspector::isLoading) {
    auto v = MessagePayloadSerializer{};
    auto res = f.apply(v);
    x = MessagePayload{v};
    return res;
  } else {
    auto v = MessagePayloadSerializer{x};
    return f.apply(v);
  }
}

struct ModernMessage {
  ExecutionNumber executionNumber;
  MessagePayload payload;
};
template<typename Inspector>
auto inspect(Inspector& f, ModernMessage& x) {
  return f.object(x).fields(
      f.field(Utils::executionNumberKey, x.executionNumber),
      f.field("payload", x.payload));
}

}  // namespace arangodb::pregel
