////////////////////////////////////////////////////////////////////////////////
/// @brief http server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpServer.h"

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/WorkMonitor.h"
#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpCommTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpListenTask.h"
#include "HttpServer/HttpServerJob.h"
#include "Rest/EndpointList.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief map of ChunkedTask
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<uint64_t, HttpCommTask *> HttpCommTaskMap;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the above map
////////////////////////////////////////////////////////////////////////////////

static Mutex HttpCommTaskMapLock;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class HttpServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

int HttpServer::sendChunk(uint64_t taskId, std::string const &data) {
  MUTEX_LOCKER(HttpCommTaskMapLock);

  auto &&it = HttpCommTaskMap.find(taskId);

  if (it == HttpCommTaskMap.end()) {
    return TRI_ERROR_TASK_NOT_FOUND;
  }

  std::unique_ptr<TaskData> taskData(new TaskData());

  taskData->_taskId = taskId;
  taskData->_loop = it->second->eventLoop();
  taskData->_type = TaskData::TASK_DATA_CHUNK;
  taskData->_data = data;

  Scheduler::SCHEDULER->signalTask(taskData);

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server with dispatcher and job manager
////////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer(Scheduler *scheduler, Dispatcher *dispatcher,
                       HttpHandlerFactory *handlerFactory,
                       AsyncJobManager *jobManager, double keepAliveTimeout)
    : _scheduler(scheduler),
      _dispatcher(dispatcher),
      _handlerFactory(handlerFactory),
      _jobManager(jobManager),
      _listenTasks(),
      _endpointList(nullptr),
      _commTasks(),
      _keepAliveTimeout(keepAliveTimeout) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer() {
  for (auto &task : _commTasks) {
    unregisterChunkedTask(task);
    _scheduler->destroyTask(task);
  }

  stopListening();
}

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a suitable communication task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask *HttpServer::createCommTask(TRI_socket_t s,
                                         ConnectionInfo const &info) {
  return new HttpCommTask(this, s, info, _keepAliveTimeout);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add the endpoint list
////////////////////////////////////////////////////////////////////////////////

void HttpServer::setEndpointList(EndpointList const *list) {
  _endpointList = list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::startListening() {
  auto endpoints = _endpointList->getByPrefix(encryptionType());

  for (auto &&i : endpoints) {
    LOG_TRACE("trying to bind to endpoint '%s' for requests", i.first.c_str());

    bool ok = openEndpoint(i.second);

    if (ok) {
      LOG_DEBUG("bound to endpoint '%s'", i.first.c_str());
    } else {
      LOG_FATAL_AND_EXIT(
          "failed to bind to endpoint '%s'. Please check "
          "whether another instance is already running or "
          "review your endpoints configuration.",
          i.first.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stopListening() {
  for (auto &task : _listenTasks) {
    _scheduler->destroyTask(task);
  }

  _listenTasks.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a chunked task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::registerChunkedTask(HttpCommTask *task, ssize_t n) {
  auto id = task->taskId();
  MUTEX_LOCKER(HttpCommTaskMapLock);

  HttpCommTaskMap[id] = task;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a chunked task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::unregisterChunkedTask(HttpCommTask *task) {
  auto id = task->taskId();
  MUTEX_LOCKER(HttpCommTaskMapLock);

  HttpCommTaskMap.erase(id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stop() {
  while (true) {
    HttpCommTask *task = nullptr;

    {
      MUTEX_LOCKER(_commTasksLock);

      if (_commTasks.empty()) {
        break;
      }

      task = *_commTasks.begin();
      _commTasks.erase(task);
    }

    unregisterChunkedTask(task);
    _scheduler->destroyTask(task);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleConnected(TRI_socket_t s, ConnectionInfo const &info) {
  HttpCommTask *task = createCommTask(s, info);

  try {
    MUTEX_LOCKER(_commTasksLock);
    _commTasks.emplace(task);
  } catch (...) {
    // destroy the task to prevent a leak
    deleteTask(task);
    throw;
  }

  // registers the task and get the number of the scheduler thread
  ssize_t n;
  int res = _scheduler->registerTask(task, &n);

  // register the ChunkedTask in the same thread
  if (res == TRI_ERROR_NO_ERROR) {
    registerChunkedTask(task, n);
  }

  task->setupDone();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection close
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationClosed(HttpCommTask *task) {
  {
    MUTEX_LOCKER(_commTasksLock);
    _commTasks.erase(task);
  }

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationFailure(HttpCommTask *task) {
  {
    MUTEX_LOCKER(_commTasksLock);
    _commTasks.erase(task);
  }

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequestAsync(WorkItem::uptr<HttpHandler> &handler,
                                    uint64_t *jobId) {

  // extract the coordinator flag
  bool found;
  char const* hdr = handler->getRequest()->header("x-arango-coordinator", found);

  if (! found) {
    hdr = nullptr;
  }

  // execute the handler using the dispatcher
  std::unique_ptr<Job> job = std::make_unique<HttpServerJob>(this, handler, true);

  // register the job with the job manager
  if (jobId != nullptr) {
    _jobManager->initAsyncJob(static_cast<HttpServerJob *>(job.get()), hdr);
    *jobId = job->jobId();
  }

  // execute the handler using the dispatcher
  int res = _dispatcher->addJob(job);

  // could not add job to job queue
  if (res != TRI_ERROR_NO_ERROR) {
    job->requestStatisticsAgentSetExecuteError();
    LOG_WARNING("unable to add job to the job queue: %s",
                TRI_errno_string(res));
    // todo send info to async work manager?
    return false;
  }

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequest(HttpCommTask *task,
                               WorkItem::uptr<HttpHandler> &handler) {
  // direct handlers
  if (handler->isDirect()) {
    HandlerWorkStack work(handler);
    handleRequestDirectly(task, work.handler());

    return true;
  }

  // use a dispatcher queue, handler belongs to the job
  std::unique_ptr<Job> job = std::make_unique<HttpServerJob>(this, handler);

  // add the job to the dispatcher
  int res = _dispatcher->addJob(job);

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::openEndpoint(Endpoint *endpoint) {
  ListenTask *task = new HttpListenTask(this, endpoint);

  // ...................................................................
  // For some reason we have failed in our endeavour to bind to the socket -
  // this effectively terminates the server
  // ...................................................................

  if (!task->isBound()) {
    deleteTask(task);
    return false;
  }

  _scheduler->registerTask(task);
  _listenTasks.emplace_back(task);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleRequestDirectly(HttpCommTask *task,
                                       HttpHandler *handler) {
  handler->executeFull();
  task->handleResponse(handler->getResponse());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
