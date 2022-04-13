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

#include "Workload.h"
#include "Execution.h"

namespace arangodb::sepp::workloads {

struct InsertDocuments : Workload {
  using Workload::Workload;

  std::unique_ptr<ExecutionThread> createThread(std::uint32_t id,
                                                Execution const& exec,
                                                Server& server) override;
  struct Thread : ExecutionThread {
    using ExecutionThread::ExecutionThread;
    ~Thread();
    void run() override;
    [[nodiscard]] virtual ThreadReport report() const override {
      return {.operations = _operations};
    }

   private:
    std::uint64_t _operations{0};
  };
};
}  // namespace arangodb::sepp::workloads