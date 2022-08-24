////////////////////////////////////////////////////////////////////////////////
///
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

#include <Algorithm/Algorithm.h>

struct IWorker {
  virtual auto loadGraphShard() -> void = 0;
  virtual auto runGSS() -> void = 0;
  virtual auto storeGraphShard() -> void = 0;

  virtual ~IWorker() = default;
};

template <typename Algorithm> struct Worker : IWorker {
  //  GraphStorage<...> graphShard;

  auto loadGraphShard() -> void override {
    fmt::print("Loading ma graph shard");
    return;
  }
  auto runGSS() -> void override { return; }
  auto storeGraphShard() -> void override { return; }
};

template <typename Algorithm>
auto makeWorker() -> std::shared_ptr<Worker<Algorithm>> {
  return std::make_shared<Worker<Algorithm>>();
}
