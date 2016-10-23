////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_JOB_H
#define ARANGODB_PREGEL_JOB_H 1

#include "Basics/Common.h"
#include "Dispatcher/Job.h"
#include "WorkerContext.h"

namespace arangodb {
    class SingleCollectionTransaction;
namespace pregel {
    class Worker;
  
  class WorkerJob : public rest::Job {
    WorkerJob(WorkerJob const&) = delete;
    WorkerJob& operator=(WorkerJob const&) = delete;
    
  public:
    WorkerJob(Worker *worker, std::shared_ptr<WorkerContext> ctx);
    
    void work() override;
    bool cancel() override;
    void cleanup(rest::DispatcherQueue*) override;
    void handleError(basics::Exception const& ex) override;

  private:
    std::atomic<bool> _canceled;
    Worker *_worker;
      std::shared_ptr<WorkerContext> _ctx;
  };
}
}
#endif
