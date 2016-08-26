////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "GeneralServerJob.h"

#include "Basics/WorkMonitor.h"
#include "Dispatcher/DispatcherQueue.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

GeneralServerJob::GeneralServerJob(GeneralServer* server,
                                   WorkItem::uptr<RestHandler> handler,
                                   bool isAsync)
    : Job("GeneralServerJob"),
      _server(server),
      _workDesc(nullptr),
      _isAsync(isAsync) {
  _handler = std::move(handler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

GeneralServerJob::~GeneralServerJob() {
  if (_workDesc != nullptr) {
    WorkMonitor::freeWorkDescription(_workDesc);
  }
}

size_t GeneralServerJob::queue() const { return _handler->queue(); }

void GeneralServerJob::work() {
  TRI_ASSERT(_handler.get() != nullptr);
  RequestStatisticsAgent::transferTo(_handler.get());

  LOG(TRACE) << "beginning job " << (void*)this;

  // the _handler needs to stay intact, so that we can cancel the job
  // therefore cannot use HandlerWorkStack here. Because we need to
  // keep the handler until the job is destroyed. Note that destroying
  // might happen in a different thread in case of shutdown.

  WorkMonitor::pushHandler(_handler.get());

  try {
    _handler->executeFull();

    if (_isAsync) {
      _handler->RequestStatisticsAgent::release();
      GeneralServerFeature::JOB_MANAGER->finishAsyncJob(
          _jobId, _handler->stealResponse());
    } else {
      auto data = std::make_unique<TaskData>();

      data->_taskId = _handler->taskId();
      data->_loop = _handler->eventLoop();
      data->_type = TaskData::TASK_DATA_RESPONSE;
      data->_response = _handler->stealResponse();

      _handler->RequestStatisticsAgent::transferTo(data.get());

      SchedulerFeature::SCHEDULER->signalTask(data);
    }

    LOG(TRACE) << "finished job " << (void*)this;
  } catch (...) {
    _handler->requestStatisticsAgentSetExecuteError();
    _workDesc = WorkMonitor::popHandler(_handler.release(), false);
    throw;
  }

  _workDesc = WorkMonitor::popHandler(_handler.release(), false);
}

bool GeneralServerJob::cancel() { return _handler->cancel(); }

void GeneralServerJob::cleanup(DispatcherQueue* queue) {
  queue->removeJob(this);
  delete this;
}

void GeneralServerJob::handleError(arangodb::basics::Exception const& ex) {
  _handler->handleError(ex);
}
