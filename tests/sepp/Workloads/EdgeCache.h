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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>

#include "Inspection/Status.h"
#include "Inspection/Types.h"

#include "ExecutionThread.h"
#include "StoppingCriterion.h"
#include "ValueGenerator.h"
#include "Workload.h"

namespace arangodb::sepp::workloads {

struct EdgeCache : Workload {
  struct ThreadOptions;
  struct Options;
  struct Thread;

  EdgeCache(Options const& options) : _options(options) {}

  auto createThreads(Execution& exec, Server& server)
      -> WorkerThreadList override;
  auto stoppingCriterion() const noexcept -> StoppingCriterion::type override;

 private:
  Options const& _options;
};

struct EdgeCache::Options {
  struct Thread {
    std::string collection;
    std::uint32_t documentsPerTrx;
    std::uint32_t readsPerEdge;
    std::uint64_t edgesPerVertex;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Thread& o) {
      return f.object(o).fields(
          f.field("documentsPerTrx", o.documentsPerTrx).fallback(100u),
          f.field("edgesPerVertex", o.edgesPerVertex).fallback(10u),
          f.field("readsPerEdge", o.readsPerEdge).fallback(2u),
          f.field("collection", o.collection));
    }
  };

  std::optional<Thread> defaultThreadOptions;
  std::uint32_t threads{1};
  StoppingCriterion::type stop;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, Options& o) {
    return f.object(o).fields(f.field("default", o.defaultThreadOptions),
                              f.field("threads", o.threads),
                              f.field("stopAfter", o.stop));
  }
};

struct EdgeCache::ThreadOptions {
  std::string collection;
  std::uint32_t documentsPerTrx{100};
  std::uint64_t readsPerEdge{2};
  std::uint64_t edgesPerVertex{10};
  StoppingCriterion::type stop;
};

struct EdgeCache::Thread : ExecutionThread {
  Thread(ThreadOptions options, std::uint32_t id, Execution& exec,
         Server& server);
  ~Thread();

  void run() override;
  [[nodiscard]] auto report() const -> ThreadReport override;
  auto shouldStop() const noexcept -> bool override;

 private:
  void executeWriteTransaction();
  void executeReadTransaction(std::uint64_t startDocument);
  void buildString(std::uint64_t currentDocument);
  ThreadOptions _options;
  std::uint64_t _operations{0};
  std::uint64_t currentDocument{0};

  // string prefix used for edges in this thread
  std::string _prefix;
  // temporary string object used to build string values
  std::string scratch;
};

}  // namespace arangodb::sepp::workloads
