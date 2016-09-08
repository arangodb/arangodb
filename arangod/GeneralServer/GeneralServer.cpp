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

#include "GeneralServer.h"

#include "Basics/MutexLocker.h"
#include "Basics/WorkMonitor.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Endpoint/EndpointList.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralListenTask.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/GeneralServerJob.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/CommonDefines.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

int GeneralServer::sendChunk(uint64_t taskId, std::string const& data) {
  auto taskData = std::make_unique<TaskData>();

  taskData->_taskId = taskId;
  taskData->_loop = SchedulerFeature::SCHEDULER->lookupLoopById(taskId);
  taskData->_type = TaskData::TASK_DATA_CHUNK;
  taskData->_data = data;

  SchedulerFeature::SCHEDULER->signalTask(taskData);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

GeneralServer::~GeneralServer() { stopListening(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief add the endpoint list
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::setEndpointList(EndpointList const* list) {
  _endpointList = list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listening
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::startListening() {
  for (auto& it : _endpointList->allEndpoints()) {
    LOG(TRACE) << "trying to bind to endpoint '" << it.first
               << "' for requests";

    bool ok = openEndpoint(it.second);

    if (ok) {
      LOG(DEBUG) << "bound to endpoint '" << it.first << "'";
    } else {
      LOG(FATAL) << "failed to bind to endpoint '" << it.first
                 << "'. Please check whether another instance is already "
                    "running using this endpoint and review your endpoints "
                    "configuration.";
      FATAL_ERROR_EXIT();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::stopListening() {
  for (auto& task : _listenTasks) {
    SchedulerFeature::SCHEDULER->destroyTask(task);
  }

  _listenTasks.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::handleRequestAsync(GeneralCommTask* task,
                                       WorkItem::uptr<RestHandler> handler,
                                       uint64_t* jobId) {
  auto messageId = handler->request()->messageId();
  bool startThread = handler->needsOwnThread();

  // extract the coordinator flag
  bool found;
  std::string const& hdrStr =
      handler->request()->header(StaticStrings::Coordinator, found);
  char const* hdr = found ? hdrStr.c_str() : nullptr;

  // execute the handler using the dispatcher
  std::unique_ptr<Job> job =
      std::make_unique<GeneralServerJob>(this, std::move(handler), true);
  task->getAgent(messageId)->transferTo(job.get());

  // register the job with the job manager
  if (jobId != nullptr) {
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(
        static_cast<GeneralServerJob*>(job.get()), hdr);
    *jobId = job->jobId();
  }

  // execute the handler using the dispatcher
  int res = DispatcherFeature::DISPATCHER->addJob(job, startThread);

  // could not add job to job queue
  if (res != TRI_ERROR_NO_ERROR) {
    job->requestStatisticsAgentSetExecuteError();
    job->RequestStatisticsAgent::transferTo(task->getAgent(messageId));
    if (res != TRI_ERROR_DISPATCHER_IS_STOPPING) {
      LOG(WARN) << "unable to add job to the job queue: "
                << TRI_errno_string(res);
      task->handleSimpleError(rest::ResponseCode::SERVICE_UNAVAILABLE, res, TRI_errno_string(res), messageId);
    } else {
      task->handleSimpleError(rest::ResponseCode::SERVICE_UNAVAILABLE, messageId);
      return true;
    }
    // TODO send info to async work manager?
    return false;
  }

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::handleRequest(GeneralCommTask* task,
                                  WorkItem::uptr<RestHandler> handler) {
  TRI_ASSERT(handler != nullptr);

  // direct handlers
  if (handler->isDirect()) {
    handleRequestDirectly(task, std::move(handler));
    return true;
  }

  bool startThread = handler->needsOwnThread();
  auto messageId = handler->request()->messageId();

  // use a dispatcher queue, handler belongs to the job
  std::unique_ptr<Job> job =
      std::make_unique<GeneralServerJob>(this, std::move(handler));
  task->getAgent(messageId)->transferTo(job.get());

  LOG(TRACE) << "GeneralCommTask " << (void*)task
             << " created GeneralServerJob " << (void*)job.get();

  // add the job to the dispatcher
  int res = DispatcherFeature::DISPATCHER->addJob(job, startThread);

  if (res != TRI_ERROR_NO_ERROR) {
    task->handleSimpleError(rest::ResponseCode::SERVICE_UNAVAILABLE, res, TRI_errno_string(res), messageId);

    return true;
  }

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::openEndpoint(Endpoint* endpoint) {
  ProtocolType protocolType;

  if (endpoint->transport() == Endpoint::TransportType::HTTP) {
    if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
      protocolType = ProtocolType::HTTPS;
    } else {
      protocolType = ProtocolType::HTTP;
    }
  } else {
    if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
      protocolType = ProtocolType::VPPS;
    } else {
      protocolType = ProtocolType::VPP;
    }
  }

  ListenTask* task = new GeneralListenTask(this, endpoint, protocolType);

  // ...................................................................
  // For some reason we have failed in our endeavor to bind to the socket -
  // this effectively terminates the server
  // ...................................................................

  if (!task->isBound()) {
    deleteTask(task);
    return false;
  }

  int res = SchedulerFeature::SCHEDULER->registerTask(task);

  if (res == TRI_ERROR_NO_ERROR) {
    _listenTasks.emplace_back(task);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::handleRequestDirectly(GeneralCommTask* task,
                                          WorkItem::uptr<RestHandler> handler) {
  uint64_t messageId = 0UL;
  auto req = handler->request();
  auto res = handler->response();
  if (req) {
    messageId = req->messageId();
  } else if (res) {
    messageId = res->messageId();
  } else {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  HandlerWorkStack work(std::move(handler));
  task->getAgent(messageId)->transferTo(work.handler());
  RestHandler::status result = work.handler()->executeFull();
  work.handler()->RequestStatisticsAgent::transferTo(task->getAgent(messageId));

  switch (result) {
    case RestHandler::status::FAILED:
    case RestHandler::status::DONE: {
      task->addResponse(work.handler()->response());
      break;
    }

    case RestHandler::status::ASYNC:
      handler.release();
      break;
  }
}
