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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <limits>
#include <optional>
#include <variant>

#include "Inspection/Status.h"
#include "Inspection/Types.h"

#include "ExecutionThread.h"
#include "Inspection/detail/traits.h"
#include "StoppingCriterion.h"
#include "Workload.h"

namespace arangodb::sepp::workloads {

struct GetByPrimaryKey : Workload {
  struct ThreadOptions {
    std::string keyPrefix;
    std::uint64_t minNumericKeyValue{0};
    std::uint64_t maxNumericKeyValue{std::numeric_limits<std::uint64_t>::max()};
    bool fillBlockCache{false};
    bool fetchFullDocument{true};
    std::string collection;
    StoppingCriterion::type stop;
  };
  struct Options;
  struct Thread;
  struct Object;

  GetByPrimaryKey(Options const& options) : _options(options) {}

  auto createThreads(Execution& exec, Server& server)
      -> WorkerThreadList override;
  auto stoppingCriterion() const noexcept -> StoppingCriterion::type override;

 private:
  Options const& _options;
};

struct GetByPrimaryKey::Options {
  struct Thread {
    std::string keyPrefix;
    std::uint64_t minNumericKeyValue{0};
    std::uint64_t maxNumericKeyValue{std::numeric_limits<std::uint64_t>::max()};
    bool fillBlockCache{false};
    bool fetchFullDocument{true};
    std::string collection;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Thread& o) {
      return f.object(o).fields(
          f.field("keyPrefix", o.keyPrefix),
          f.field("minNumericKeyValue", o.minNumericKeyValue),
          f.field("maxNumericKeyValue", o.maxNumericKeyValue),
          f.field("fillBlockCache", o.fillBlockCache).fallback(f.keep()),
          f.field("fetchFullDocument", o.fetchFullDocument).fallback(f.keep()),
          f.field("collection", o.collection));
    }
  };

  std::optional<Thread> defaultThreadOptions;
  std::uint32_t threads{1};  // TODO - make variant fixed number/array of Thread
  StoppingCriterion::type stop;
};

template<class Inspector>
inline auto inspect(Inspector& f, GetByPrimaryKey::Options& o) {
  return f.object(o).fields(f.field("default", o.defaultThreadOptions),
                            f.field("threads", o.threads),
                            f.field("stopAfter", o.stop));
}

struct GetByPrimaryKey::Thread : ExecutionThread {
  Thread(ThreadOptions options, Execution& exec, Server& server);
  ~Thread();
  void run() override;
  [[nodiscard]] virtual ThreadReport report() const override {
    return {.data = {}, .operations = _operations};
  }
  auto shouldStop() const noexcept -> bool override;

 private:
  std::uint64_t _operations{0};
  ThreadOptions _options;
};

}  // namespace arangodb::sepp::workloads
