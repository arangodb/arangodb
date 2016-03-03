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
#include "Basics/Logger.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpCommTask.h"
#include "HttpServer/ArangoTask.h"
#include "HttpServer/ArangoTask.h"
#include "HttpServer/GeneralHandler.h"
#include "HttpServer/HttpListenTask.h"
#include "HttpServer/GeneralServerJob.h"
#include "Rest/EndpointList.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"
#include "VelocyServer/VelocyCommTask.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

int GeneralServer::sendChunk(uint64_t taskId, std::string const& data) {
  std::unique_ptr<TaskData> taskData(new TaskData());

  taskData->_taskId = taskId;
  taskData->_loop = Scheduler::SCHEDULER->lookupLoopById(taskId);
  taskData->_type = TaskData::TASK_DATA_CHUNK;
  taskData->_data = data;

  Scheduler::SCHEDULER->signalTask(taskData);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server with dispatcher and job manager
////////////////////////////////////////////////////////////////////////////////

GeneralServer::GeneralServer(Scheduler* scheduler, Dispatcher* dispatcher,
                       GeneralHandlerFactory* handlerFactory,
                       AsyncJobManager* jobManager, double keepAliveTimeout)
    : _scheduler(scheduler),
      _dispatcher(dispatcher),
      _handlerFactory(handlerFactory),
      _jobManager(jobManager),
      _listenTasks(),
      _endpointList(nullptr),
      _commTasks(),
      _commTasksVstream(),
      _keepAliveTimeout(keepAliveTimeout) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

GeneralServer::~GeneralServer() { stopListening(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a suitable communication task
////////////////////////////////////////////////////////////////////////////////

ArangoTask* GeneralServer::createCommTask(TRI_socket_t s,
                                         ConnectionInfo const& info) {
  return new HttpCommTask(this, s, info, _keepAliveTimeout);
}

// Overload for VelocyStream

VelocyCommTask* GeneralServer::createCommTask(TRI_socket_t s, 
                                         ConnectionInfo const &info, bool _isHttp) {
  return new VelocyCommTask(this, s, info, _keepAliveTimeout);
}

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
  auto endpoints = _endpointList->getByPrefix(encryptionType());

  for (auto&& i : endpoints) {
    LOG(TRACE) << "trying to bind to endpoint '" << i.first << "' for requests";

    bool ok = openEndpoint(i.second);

    if (ok) {
      LOG(DEBUG) << "bound to endpoint '" << i.first << "'";
    } else {
      LOG(FATAL) << "failed to bind to endpoint '" << i.first << "'. Please check whether another instance is already running or review your endpoints configuration."; FATAL_ERROR_EXIT();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listening
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::stopListening() {
  for (auto& task : _listenTasks) {
    _scheduler->destroyTask(task);
  }

  _listenTasks.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::stop() {
  while (true) {
    if(_isHttp){
      ArangoTask* task = nullptr;

// <<<<<<< HEAD:arangod/HttpServer/GeneralServer.cpp
//         if (_commTasks.empty()) {
//           break;
//         }
// =======
      MUTEX_LOCKER(mutexLocker, _commTasksLock);
// >>>>>>> upstream/devel:arangod/HttpServer/HttpServer.cpp

      task = *_commTasks.begin();
      _commTasks.erase(task);

      _scheduler->destroyTask(task);
    }else{
      VelocyCommTask* task_v = nullptr;
      {
        MUTEX_LOCKER(mutexLocker, _commTasksLock);

        if (_commTasksVstream.empty()) {
          break;
        }

        task_v = *_commTasksVstream.begin();
        _commTasksVstream.erase(task_v);
      }

      _scheduler->destroyTask(task_v);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::handleConnected(TRI_socket_t s, ConnectionInfo const& info, bool isHttp) {
  _isHttp = isHttp;
  if(_isHttp){
    ArangoTask* task = createCommTask(s, info);
    try {
      MUTEX_LOCKER(mutexLocker, _commTasksLock);
      _commTasks.emplace(task);
    } catch (...) {
      // destroy the task to prevent a leak
      deleteTask(task);
      throw;
    }

// <<<<<<< HEAD:arangod/HttpServer/GeneralServer.cpp
//     // registers the task and get the number of the scheduler thread
//     ssize_t n;
//     _scheduler->registerTask(task, &n);
// =======
  try {
    MUTEX_LOCKER(mutexLocker, _commTasksLock);
    _commTasks.emplace(task);
  } catch (...) {
    // destroy the task to prevent a leak
    deleteTask(task);
    throw;
  }
// >>>>>>> upstream/devel:arangod/HttpServer/HttpServer.cpp

  } else{
    VelocyCommTask* task_v = createCommTask(s, info, _isHttp);
    try {
      MUTEX_LOCKER(mutexLocker, _commTasksLock);
      _commTasksVstream.emplace(task_v);
    } catch (...) {
      // destroy the task to prevent a leak
      deleteTask(task_v);
      throw;
    }

    // registers the task and get the number of the scheduler thread
    ssize_t n;
    _scheduler->registerTask(task_v, &n);
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection close
////////////////////////////////////////////////////////////////////////////////

// <<<<<<< HEAD:arangod/HttpServer/GeneralServer.cpp
void GeneralServer::handleCommunicationClosed(ArangoTask* task) {
  // MUTEX_LOCKER(_commTasksLock);
// =======
// void HttpServer::handleCommunicationClosed(HttpCommTask* task) {
  MUTEX_LOCKER(mutexLocker, _commTasksLock);
// >>>>>>> upstream/devel:arangod/HttpServer/HttpServer.cpp
  _commTasks.erase(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

// <<<<<<< HEAD:arangod/HttpServer/GeneralServer.cpp
void GeneralServer::handleCommunicationFailure(ArangoTask* task) {
  // MUTEX_LOCKER(_commTasksLock);
// =======
// void HttpServer::handleCommunicationFailure(HttpCommTask* task) {
  MUTEX_LOCKER(mutexLocker, _commTasksLock);
// >>>>>>> upstream/devel:arangod/HttpServer/HttpServer.cpp
  _commTasks.erase(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::handleRequestAsync(WorkItem::uptr<GeneralHandler>& handler,
                                    uint64_t* jobId) {
  // extract the coordinator flag
  bool found;
  char const* hdr =
      handler->getRequest()->header("x-arango-coordinator", found);

  if (!found) {
    hdr = nullptr;
  }

  // execute the handler using the dispatcher
  std::unique_ptr<Job> job =
      std::make_unique<GeneralServerJob>(this, handler, true); //@TODO: To be changed to GeneralServerJob

  // register the job with the job manager
  if (jobId != nullptr) {
    _jobManager->initAsyncJob(static_cast<GeneralServerJob*>(job.get()), hdr);
    *jobId = job->jobId();
  }

  // execute the handler using the dispatcher
  int res = _dispatcher->addJob(job);

  // could not add job to job queue
  if (res != TRI_ERROR_NO_ERROR) {
    job->requestStatisticsAgentSetExecuteError();
    LOG(WARN) << "unable to add job to the job queue: " << TRI_errno_string(res);
    // todo send info to async work manager?
    return false;
  }

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::handleRequest(ArangoTask* task,
                               WorkItem::uptr<GeneralHandler>& handler) {
  // direct handlers
  if (handler->isDirect()) {
    HandlerWorkStack work(handler);
    handleRequestDirectly(task, work.handler());

    return true;
  }

  // use a dispatcher queue, handler belongs to the job
  std::unique_ptr<Job> job = std::make_unique<GeneralServerJob>(this, handler);

  LOG(TRACE) << "HttpCommTask " << (void*)task << " created HttpServerJob " << (void*)job.get();

  // add the job to the dispatcher
  int res = _dispatcher->addJob(job);

  // job is in queue now
  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

bool GeneralServer::openEndpoint(Endpoint* endpoint) {
  ListenTask* task = new HttpListenTask(this, endpoint);

  // ...................................................................
  // For some reason we have failed in our endeavour to bind to the socket -
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
/// @brief handle request directly (Http)
////////////////////////////////////////////////////////////////////////////////

void GeneralServer::handleRequestDirectly(ArangoTask* task,
                                       GeneralHandler* handler) {
  GeneralHandler::status_t status = handler->executeFull();

  switch (status._status) {
    case GeneralHandler::HANDLER_FAILED:
    case GeneralHandler::HANDLER_DONE:
      task->handleResponse(handler->getResponse());
      break;

    case GeneralHandler::HANDLER_ASYNC:
      // do nothing, just wait
      break;
  }
}