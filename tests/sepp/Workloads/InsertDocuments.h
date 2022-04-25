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

#include <optional>
#include <variant>

#include "Inspection/Status.h"
#include "Inspection/Types.h"

#include "ExecutionThread.h"
#include "Workload.h"
#include "velocypack/SliceContainer.h"

namespace arangodb::sepp::workloads {

struct InsertDocuments : Workload {
  struct ThreadOptions {
    std::string collection;
    std::shared_ptr<velocypack::Builder> object;
  };
  struct Options;
  struct Thread;
  struct Object;

  InsertDocuments(Options const& options) : _options(options) {}

  WorkerThreadList createThreads(Execution const& exec,
                                 Server& server) override;

 private:
  Options const& _options;
};

struct InsertDocuments::Object : std::variant<std::string, velocypack::Slice> {
  template<class Inspector>
  friend inline auto inspect(Inspector& f, Object& o) {
    namespace insp = arangodb::inspection;
    return f.variant(o)
        .qualified("source", "value")
        .alternatives(insp::type<std::string>("file"),
                      insp::type<velocypack::Slice>("inline"));
  }
};

struct InsertDocuments::Options {
  struct Thread {
    std::string collection;
    Object object;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Thread& o) {
      return f.object(o).fields(f.field("object", o.object),
                                f.field("collection", o.collection));
    }
  };

  std::optional<Thread> defaultThreadOptions;
  std::uint32_t threads{1};  // TODO - make variant fixed number/array of Thread

  template<class Inspector>
  friend inline auto inspect(Inspector& f, Options& o) {
    return f.object(o).fields(f.field("default", o.defaultThreadOptions),
                              f.field("threads", o.threads));
  }
};

struct InsertDocuments::Thread : ExecutionThread {
  Thread(ThreadOptions options, Execution const& exec, Server& server);
  ~Thread();
  void run() override;
  [[nodiscard]] virtual ThreadReport report() const override {
    return {.operations = _operations};
  }

 private:
  std::uint64_t _operations{0};
  ThreadOptions _options;
};

}  // namespace arangodb::sepp::workloads