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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <Inspection/VPack.h>
#include <Inspection/Transformers.h>

#include <Pregel/Common.h>

namespace arangodb::pregel {

template<typename T>
auto add(std::optional<T> const& a, std::optional<T> const& b)
    -> std::optional<T> {
  if (a.has_value() && b.has_value()) {
    return {a.value() + b.value()};
  } else if (!a.has_value() && !b.has_value()) {
    return {std::nullopt};
  } else if (!a.has_value()) {
    return {b};
  } else {
    return {a};
  }
}

struct GssStatus {
  size_t verticesProcessed = 0;
  size_t messagesSent = 0;
  size_t messagesReceived = 0;
  size_t memoryBytesUsedForMessages = 0;
  bool operator==(GssStatus const&) const = default;
  auto operator+(GssStatus const& other) const -> GssStatus {
    auto status = GssStatus{
        .verticesProcessed = verticesProcessed + other.verticesProcessed,
        .messagesSent = messagesSent + other.messagesSent,
        .messagesReceived = messagesReceived + other.messagesReceived,
        .memoryBytesUsedForMessages =
            memoryBytesUsedForMessages + other.memoryBytesUsedForMessages};
    return status;
  }
  auto isDefault() const -> bool { return *this == GssStatus{}; }
};

template<typename Inspector>
auto inspect(Inspector& f, GssStatus& x) {
  return f.object(x).fields(
      f.field("verticesProcessed", x.verticesProcessed),
      f.field("messagesSent", x.messagesSent),
      f.field("messagesReceived", x.messagesReceived),
      f.field("memoryBytesUsedForMessages", x.memoryBytesUsedForMessages));
}

struct AllGssStatus {
  std::vector<GssStatus> gss;
  bool operator==(AllGssStatus const&) const = default;
  auto operator+(AllGssStatus const& other) const -> AllGssStatus {
    AllGssStatus sum;
    auto minSize = std::min(gss.size(), other.gss.size());
    for (unsigned int i = 0; i < minSize; ++i) {
      sum.gss.emplace_back(gss[i] + other.gss[i]);
    }
    return sum;
  }
  auto push(GssStatus const& status) -> void { gss.emplace_back(status); }
};

template<typename Inspector>
auto inspect(Inspector& f, AllGssStatus& x) {
  return f.object(x).fields(f.field("items", x.gss));
}

struct GraphStoreStatus {
  std::optional<std::size_t> verticesLoaded = std::nullopt;
  std::optional<std::size_t> edgesLoaded = std::nullopt;
  std::optional<std::size_t> memoryBytesUsed = std::nullopt;
  std::optional<std::size_t> verticesStored = std::nullopt;
  bool operator==(GraphStoreStatus const&) const = default;
  auto operator+(GraphStoreStatus const& other) const -> GraphStoreStatus {
    return GraphStoreStatus{
        .verticesLoaded = add(verticesLoaded, other.verticesLoaded),
        .edgesLoaded = add(edgesLoaded, other.edgesLoaded),
        .memoryBytesUsed = add(memoryBytesUsed, other.memoryBytesUsed),
        .verticesStored = add(verticesStored, other.verticesStored)};
  };
};

template<typename Inspector>
auto inspect(Inspector& f, GraphStoreStatus& x) {
  return f.object(x).fields(f.field("verticesLoaded", x.verticesLoaded),
                            f.field("edgesLoaded", x.edgesLoaded),
                            f.field("memoryBytesUsed", x.memoryBytesUsed),
                            f.field("verticesStored", x.verticesStored));
}

struct Status {
  TimeStamp timeStamp{std::chrono::system_clock::now()};
  GraphStoreStatus graphStoreStatus = GraphStoreStatus{};
  std::optional<AllGssStatus> allGssStatus = std::nullopt;
  bool operator==(Status const&) const = default;
  auto operator+(Status const& other) const -> Status {
    return Status{.timeStamp = std::max(timeStamp, other.timeStamp),
                  .graphStoreStatus = graphStoreStatus + other.graphStoreStatus,
                  .allGssStatus = add(allGssStatus, other.allGssStatus)};
  }
};

template<typename Inspector>
auto inspect(Inspector& f, Status& x) {
  return f.object(x).fields(
      f.field(timeStampString, x.timeStamp)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("graphStoreStatus", x.graphStoreStatus),
      f.field("allGssStatus", x.allGssStatus));
}

struct GssObservables {
  std::atomic<size_t> verticesProcessed{0};
  std::atomic<size_t> messagesSent{0};
  std::atomic<size_t> messagesReceived{0};
  std::atomic<size_t> memoryBytesUsedForMessages{0};
  auto observe() const -> GssStatus {
    return GssStatus{
        .verticesProcessed = verticesProcessed.load(),
        .messagesSent = messagesSent.load(),
        .messagesReceived = messagesReceived.load(),
        .memoryBytesUsedForMessages = memoryBytesUsedForMessages.load()};
  }
  auto zero() -> void {
    verticesProcessed = 0;
    messagesSent = 0;
    messagesReceived = 0;
    memoryBytesUsedForMessages = 0;
  }
};

struct GraphStoreObservables {
  std::atomic<std::size_t> verticesLoaded{0};
  std::atomic<std::size_t> edgesLoaded{0};
  std::atomic<std::size_t> memoryBytesUsed{0};
  std::atomic<std::size_t> verticesStored{0};
  auto observe() const -> GraphStoreStatus {
    return GraphStoreStatus{
        .verticesLoaded = verticesLoaded.load() > 0
                              ? std::optional{verticesLoaded.load()}
                              : std::nullopt,
        .edgesLoaded = edgesLoaded.load() > 0
                           ? std::optional{edgesLoaded.load()}
                           : std::nullopt,
        .memoryBytesUsed = memoryBytesUsed.load() > 0
                               ? std::optional{memoryBytesUsed.load()}
                               : std::nullopt,
        .verticesStored = verticesStored.load() > 0
                              ? std::optional{verticesStored.load()}
                              : std::nullopt};
  }
};

}  // namespace arangodb::pregel
