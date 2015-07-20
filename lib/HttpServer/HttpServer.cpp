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

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

#ifdef TRI_USE_SPIN_LOCK_GENERAL_SERVER
#define GENERAL_SERVER_LOCKER(a) SPIN_LOCKER(a)
#else
#define GENERAL_SERVER_LOCKER(a) MUTEX_LOCKER(a)
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief map of ChunkedTask
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<uint64_t, HttpCommTask*> HttpCommTaskMap;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the above map
////////////////////////////////////////////////////////////////////////////////

static Mutex HttpCommTaskMapLock;

// -----------------------------------------------------------------------------
// --SECTION--                                               class HttpServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint server
////////////////////////////////////////////////////////////////////////////////

int HttpServer::sendChunk (uint64_t taskId, const string& data) {
  MUTEX_LOCKER(HttpCommTaskMapLock);

  auto&& it = HttpCommTaskMap.find(taskId);

  if (it == HttpCommTaskMap.end()) {
    return TRI_ERROR_TASK_NOT_FOUND;
  }

  return it->second->signalChunk(data);
}
        
// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server with dispatcher and job manager
////////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer (Scheduler* scheduler,
                        Dispatcher* dispatcher,
                        HttpHandlerFactory* handlerFactory,
                        AsyncJobManager* jobManager,
                        double keepAliveTimeout)
  : _scheduler(scheduler),
    _dispatcher(dispatcher),
    _handlerFactory(handlerFactory),
    _jobManager(jobManager),
    _listenTasks(),
    _endpointList(nullptr),
    _commTasks(),
    _keepAliveTimeout(keepAliveTimeout) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer () {
  for (auto& task : _commTasks) {
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

HttpCommTask* HttpServer::createCommTask (TRI_socket_t s, const ConnectionInfo& info) {
  return new HttpCommTask(this, s, info, _keepAliveTimeout);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add the endpoint list
////////////////////////////////////////////////////////////////////////////////

void HttpServer::setEndpointList (const EndpointList* list) {
   _endpointList = list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add another endpoint at runtime
///
/// the caller must make sure this is not called in parallel
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::addEndpoint (Endpoint* endpoint) {
  bool ok = openEndpoint(endpoint);

  if (ok) {
    LOG_INFO("added endpoint '%s'", endpoint->getSpecification().c_str());
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an endpoint at runtime
///
/// the caller must make sure this is not called in parallel
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::removeEndpoint (Endpoint* endpoint) {
  for (auto task = _listenTasks.begin();  task != _listenTasks.end();  ++task) {
    if ((*task)->endpoint() == endpoint) {
      // TODO: remove commtasks for the listentask??

      _scheduler->destroyTask(*task);
      _listenTasks.erase(task);

      LOG_INFO("removed endpoint '%s'", endpoint->getSpecification().c_str());
      return true;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::startListening () {
  map<string, Endpoint*> endpoints = _endpointList->getByPrefix(encryptionType());

  for (auto&& i : endpoints) {
    LOG_TRACE("trying to bind to endpoint '%s' for requests", i.first.c_str());

    bool ok = openEndpoint(i.second);

    if (ok) {
      LOG_DEBUG("bound to endpoint '%s'", i.first.c_str());
    }
    else {
      LOG_FATAL_AND_EXIT("failed to bind to endpoint '%s'. Please check whether another instance is already running or review your endpoints configuration.", i.first.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listening
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stopListening () {
  for (auto& task : _listenTasks) {
    _scheduler->destroyTask(task);
  }

  _listenTasks.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a chunked task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::registerChunkedTask (HttpCommTask* task, ssize_t n) {
  MUTEX_LOCKER(HttpCommTaskMapLock);

  HttpCommTaskMap[task->taskId()] = task;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a chunked task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::unregisterChunkedTask (HttpCommTask* task) {
  MUTEX_LOCKER(HttpCommTaskMapLock);

  HttpCommTaskMap.erase(task->taskId());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stop () {
  while (true) {
    HttpCommTask* task = nullptr;

    {
      GENERAL_SERVER_LOCKER(_commTasksLock);
 
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

void HttpServer::handleConnected (TRI_socket_t s, const ConnectionInfo& info) {
  HttpCommTask* task = createCommTask(s, info);

  {
    GENERAL_SERVER_LOCKER(_commTasksLock);
    try {
      _commTasks.emplace(task);
    }
    catch (...) {
      throw;
    }
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

void HttpServer::handleCommunicationClosed (HttpCommTask* task) {
  shutdownHandlerByTask(task);

  {
    GENERAL_SERVER_LOCKER(_commTasksLock);
    _commTasks.erase(task);
  }

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationFailure (HttpCommTask* task) {
  shutdownHandlerByTask(task);

  {
    GENERAL_SERVER_LOCKER(_commTasksLock);
    _commTasks.erase(task);
  }

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback if the handler received a signal
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleAsync (HttpCommTask* task) {
  task->processRead();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequestAsync (std::unique_ptr<HttpHandler>& handler, 
                                     uint64_t* jobId) {
  // execute the handler using the dispatcher
  std::unique_ptr<HttpServerJob> job(new HttpServerJob(this, handler.get(), nullptr));
  // handler now belongs to the job
  auto h = handler.release();

  // now handler.get() is a nullptr and must not be accessed

  if (jobId != nullptr) {
    try {
      _jobManager->initAsyncJob(job.get(), jobId);
    }
    catch (...) {
      RequestStatisticsAgentSetExecuteError(h);
      LOG_WARNING("unable to initialize job");
      return false;
    }
  }

  int error = _dispatcher->addJob(job.get());

  if (error != TRI_ERROR_NO_ERROR) {
    // could not add job to job queue
    RequestStatisticsAgentSetExecuteError(h);
    LOG_WARNING("unable to add job to the job queue: %s", TRI_errno_string(error));
    return false;
  }

  // job now belongs to the dispatcher
  job.release();

  // job is in queue now
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequest (HttpCommTask* task, 
                                std::unique_ptr<HttpHandler>& handler) {
  // execute handler and (possibly) requeue
  while (true) {
    // directly execute the handler within the scheduler thread
    if (handler->isDirect()) {
      Handler::status_t status = handleRequestDirectly(task, handler.get());

      if (status.status != Handler::HANDLER_REQUEUE) {
        return true;
      }
    }

    // execute the handler using the dispatcher
    else {
      std::unique_ptr<HttpServerJob> job(new HttpServerJob(this, handler.get(), task));
      // handler now belongs to the job
      auto h = handler.release();
  
      h->RequestStatisticsAgent::transfer(job.get());

      task->setCurrentJob(job.get());

      if (_dispatcher->addJob(job.get()) != TRI_ERROR_NO_ERROR) {
        task->clearCurrentJob();
        return false;
      }

      // job now belongs to the dispatcher
      job.release();
      return true;
    }
  }

  // just to pacify compilers
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the http response of the handler
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleResponse (HttpCommTask* task,
                                 HttpHandler* handler) {
  HttpResponse* response = handler->getResponse();

  if (response == nullptr) {
    basics::Exception err(TRI_ERROR_INTERNAL, "no response received from handler", __FILE__, __LINE__);

    handler->handleError(err);
    response = handler->getResponse();
  }

  RequestStatisticsAgentSetRequestEnd(handler);
  handler->RequestStatisticsAgent::transfer(task);
    
  if (response != nullptr) {
    task->handleResponse(response);
  }
  else {
    LOG_ERROR("cannot get any response");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::openEndpoint (Endpoint* endpoint) {
  ListenTask* task = new HttpListenTask(this, endpoint);

  // ...................................................................
  // For some reason we have failed in our endeavour to bind to the socket -
  // this effectively terminates the server
  // ...................................................................

  if (! task->isBound()) {
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

Handler::status_t HttpServer::handleRequestDirectly (HttpCommTask* task, HttpHandler* handler) {
  Handler::status_t status(Handler::HANDLER_FAILED);

  RequestStatisticsAgentSetRequestStart(handler);

  try {
    handler->prepareExecute();

    try {
      status = handler->execute();
    }
    catch (const basics::Exception& ex) {
      RequestStatisticsAgentSetExecuteError(handler);

      handler->handleError(ex);
    }
    catch (std::exception const& ex) {
      RequestStatisticsAgentSetExecuteError(handler);

      basics::Exception err(TRI_ERROR_SYS_ERROR, ex.what(), __FILE__, __LINE__);
      handler->handleError(err);
    }
    catch (...) {
      RequestStatisticsAgentSetExecuteError(handler);

      basics::Exception err(TRI_ERROR_SYS_ERROR, __FILE__, __LINE__);
      handler->handleError(err);
    }

    handler->finalizeExecute();

    if (status.status == Handler::HANDLER_REQUEUE) {
      handler->RequestStatisticsAgent::transfer(task);
      return status;
    }

    handleResponse(task, handler);
  }
  catch (basics::Exception const& ex) {
    RequestStatisticsAgentSetExecuteError(handler);

    LOG_ERROR("caught exception: %s", DIAGNOSTIC_INFORMATION(ex));
  }
  catch (std::exception const& ex) {
    RequestStatisticsAgentSetExecuteError(handler);

    LOG_ERROR("caught exception: %s", ex.what());
  }
  catch (...) {
    RequestStatisticsAgentSetExecuteError(handler);

    LOG_ERROR("caught exception");
  }

  return status;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down a handler for a task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::shutdownHandlerByTask (Task* task) {
  auto commTask = dynamic_cast<HttpCommTask*>(task);
  TRI_ASSERT(commTask != nullptr);

  auto job = commTask->job();
  commTask->clearCurrentJob();

  if (job != nullptr) {
    job->beginShutdown();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
