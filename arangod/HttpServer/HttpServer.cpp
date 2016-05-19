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

#include "HttpServer.h"

#include "Basics/MutexLocker.h"
#include "Basics/WorkMonitor.h"
#include "Dispatcher/Dispatcher.h"
#include "Endpoint/EndpointList.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpCommTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpListenTask.h"
#include "HttpServer/HttpServerJob.h"
#include "Logger/Logger.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

int HttpServer::sendChunk(uint64_t taskId, std::string const& data) {
  auto taskData = std::make_unique<TaskData>();

  taskData->_taskId = taskId;
  taskData->_loop = SchedulerFeature::SCHEDULER->lookupLoopById(taskId);
  taskData->_type = TaskData::TASK_DATA_CHUNK;
  taskData->_data = data;

  SchedulerFeature::SCHEDULER->signalTask(taskData);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server with dispatcher and job manager
////////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer(Scheduler* scheduler, Dispatcher* dispatcher,
                       HttpHandlerFactory* handlerFactory,
                       AsyncJobManager* jobManager, double keepAliveTimeout,
                       std::vector<std::string> const& accessControlAllowOrigins)
    : _scheduler(scheduler),
      _dispatcher(dispatcher),
      _handlerFactory(handlerFactory),
      _jobManager(jobManager),
      _listenTasks(),
      _endpointList(nullptr),
      _commTasks(),
      _keepAliveTimeout(keepAliveTimeout),
      _accessControlAllowOrigins(accessControlAllowOrigins) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer() { stopListening(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a suitable communication task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask* HttpServer::createCommTask(TRI_socket_t s,
                                         ConnectionInfo&& info) {
  return new HttpCommTask(this, s, std::move(info), _keepAliveTimeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the endpoint list
////////////////////////////////////////////////////////////////////////////////

void HttpServer::setEndpointList(EndpointList const* list) {
  _endpointList = list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::startListening() {
  auto endpoints =
      _endpointList->matching(Endpoint::TransportType::HTTP, encryptionType());

  for (auto& it : endpoints) {
    LOG(TRACE) << "trying to bind to endpoint '" << it.first << "' for requests";

    bool ok = openEndpoint(it.second);

    if (ok) {
      LOG(DEBUG) << "bound to endpoint '" << it.first << "'";
    } else {
      LOG(FATAL) << "failed to bind to endpoint '" << it.first
                 << "'. Please check whether another instance is already "
                    "running using this endpint and review your endpoints configuration.";
      FATAL_ERROR_EXIT();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stopListening() {
  for (auto& task : _listenTasks) {
    _scheduler->destroyTask(task);
  }

  _listenTasks.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stop() {
  while (true) {
    HttpCommTask* task = nullptr;

    {
      MUTEX_LOCKER(mutexLocker, _commTasksLock);

      if (_commTasks.empty()) {
        break;
      }

      task = *_commTasks.begin();
      _commTasks.erase(task);
    }

    _scheduler->destroyTask(task);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleConnected(TRI_socket_t s, ConnectionInfo&& info) {
  HttpCommTask* task = createCommTask(s, std::move(info));

  try {
    MUTEX_LOCKER(mutexLocker, _commTasksLock);
    _commTasks.emplace(task);
  } catch (...) {
    // destroy the task to prevent a leak
    deleteTask(task);
    throw;
  }

  // registers the task and get the number of the scheduler thread
  ssize_t n;
  _scheduler->registerTask(task, &n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection close
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationClosed(HttpCommTask* task) {
  MUTEX_LOCKER(mutexLocker, _commTasksLock);
  _commTasks.erase(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationFailure(HttpCommTask* task) {
  MUTEX_LOCKER(mutexLocker, _commTasksLock);
  _commTasks.erase(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequestAsync(HttpCommTask* task,
                                    WorkItem::uptr<HttpHandler>& handler,
                                    uint64_t* jobId) {
  bool startThread = task->startThread();

  // extract the coordinator flag
  bool found;
  std::string const& hdrStr = handler->getRequest()->header(StaticStrings::Coordinator, found);
  char const* hdr = found ? hdrStr.c_str() : nullptr;

  // execute the handler using the dispatcher
  std::unique_ptr<Job> job =
      std::make_unique<HttpServerJob>(this, handler, true);
  task->RequestStatisticsAgent::transferTo(job.get());

  // register the job with the job manager
  if (jobId != nullptr) {
    _jobManager->initAsyncJob(static_cast<HttpServerJob*>(job.get()), hdr);
    *jobId = job->jobId();
  }

  // execute the handler using the dispatcher
  int res = _dispatcher->addJob(job, startThread);

  // could not add job to job queue
  if (res != TRI_ERROR_NO_ERROR) {
    job->requestStatisticsAgentSetExecuteError();
    job->RequestStatisticsAgent::transferTo(task);
    if (res != TRI_ERROR_DISPATCHER_IS_STOPPING) {
      LOG(WARN) << "unable to add job to the job queue: "
                << TRI_errno_string(res);
    }
    // todo send info to async work manager?
    return false;
  }

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequest(HttpCommTask* task,
                               WorkItem::uptr<HttpHandler>& handler) {
  // direct handlers
  if (handler->isDirect()) {
    HandlerWorkStack work(handler);
    handleRequestDirectly(task, work.handler());

    return true;
  }

  bool startThread = task->startThread();

  // use a dispatcher queue, handler belongs to the job
  std::unique_ptr<Job> job = std::make_unique<HttpServerJob>(this, handler);
  task->RequestStatisticsAgent::transferTo(job.get());

  LOG(TRACE) << "HttpCommTask " << (void*)task << " created HttpServerJob "
             << (void*)job.get();

  // add the job to the dispatcher

  int res = _dispatcher->addJob(job, startThread);

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::openEndpoint(Endpoint* endpoint) {
  ListenTask* task = new HttpListenTask(this, endpoint);

  // ...................................................................
  // For some reason we have failed in our endeavor to bind to the socket -
  // this effectively terminates the server
  // ...................................................................

  if (!task->isBound()) {
    deleteTask(task);
    return false;
  }

  int res = _scheduler->registerTask(task);

  if (res == TRI_ERROR_NO_ERROR) {
    _listenTasks.emplace_back(task);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleRequestDirectly(HttpCommTask* task,
                                       HttpHandler* handler) {
  task->RequestStatisticsAgent::transferTo(handler);
  HttpHandler::status_t status = handler->executeFull();
  handler->RequestStatisticsAgent::transferTo(task);

  switch (status._status) {
    case HttpHandler::HANDLER_FAILED:
    case HttpHandler::HANDLER_DONE:
      task->handleResponse(handler->getResponse());
      break;

    case HttpHandler::HANDLER_ASYNC:
      // do nothing, just wait
      break;
  }
}
