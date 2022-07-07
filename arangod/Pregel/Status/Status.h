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

struct Status {
  TimeStamp timeStamp{std::chrono::system_clock::now()};
  std::optional<std::size_t> verticesLoaded = std::nullopt;
  std::optional<std::size_t> edgesLoaded = std::nullopt;
  std::optional<std::size_t> memoryBytesUsed = std::nullopt;
  bool operator==(Status const&) const = default;
  auto operator+(Status const& other) const -> Status {
    return Status{
        .timeStamp = std::max(timeStamp, other.timeStamp),
        .verticesLoaded = add(verticesLoaded, other.verticesLoaded),
        .edgesLoaded = add(edgesLoaded, other.edgesLoaded),
        .memoryBytesUsed = add(memoryBytesUsed, other.memoryBytesUsed)};
  };
};

template<typename Inspector>
auto inspect(Inspector& f, Status& x) {
  return f.object(x).fields(
      f.field(timeStampString, x.timeStamp)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("verticesLoaded", x.verticesLoaded),
      f.field("edgesLoaded", x.edgesLoaded),
      f.field("memoryUsed", x.memoryBytesUsed));
}

struct Observables {
  std::optional<std::atomic<std::size_t>> verticesLoaded;
  std::optional<std::atomic<std::size_t>> edgesLoaded;
  std::optional<std::atomic<std::size_t>> memoryBytesUsed;
  auto observe() const -> Status {
    return Status{
        .verticesLoaded = verticesLoaded.has_value()
                              ? std::optional{verticesLoaded.value().load()}
                              : std::nullopt,
        .edgesLoaded = edgesLoaded.has_value()
                           ? std::optional{edgesLoaded.value().load()}
                           : std::nullopt,
        .memoryBytesUsed = memoryBytesUsed.has_value()
                               ? std::optional{memoryBytesUsed.value().load()}
                               : std::nullopt};
  }
};

}  // namespace arangodb::pregel
