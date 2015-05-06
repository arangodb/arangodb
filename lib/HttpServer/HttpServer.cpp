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
    _handlers(),
    _task2handler(),
    _keepAliveTimeout(keepAliveTimeout) {
  GENERAL_SERVER_INIT(&_commTasksLock);
  GENERAL_SERVER_INIT(&_mappingLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

HttpServer::~HttpServer () {
  for (auto task : _commTasks) {
    unregisterChunkedTask(task);
    _scheduler->destroyTask(task);
  }

  stopListening();

  GENERAL_SERVER_DESTROY(&_mappingLock);
  GENERAL_SERVER_DESTROY(&_commTasksLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the protocol
////////////////////////////////////////////////////////////////////////////////

const char* HttpServer::protocol () const {
  return "http";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the encryption to be used
////////////////////////////////////////////////////////////////////////////////

Endpoint::EncryptionType HttpServer::encryptionType () const {
  return Endpoint::ENCRYPTION_NONE;
}

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
/// @brief return the scheduler
////////////////////////////////////////////////////////////////////////////////

Scheduler* HttpServer::scheduler () const {
  return _scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* HttpServer::dispatcher () const {
  return _dispatcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory* HttpServer::handlerFactory () const {
  return _handlerFactory;
}

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
  for (auto task : _listenTasks) {
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
/// @brief shuts down handlers
////////////////////////////////////////////////////////////////////////////////

void HttpServer::shutdownHandlers () {
  GENERAL_SERVER_LOCK(&_mappingLock);

  for (auto&& i : _handlers) {
    HttpServerJob* job = i.second._job;

    if (job != nullptr) {
      job->abandon();
      i.second._job = nullptr;
    }
  }

  GENERAL_SERVER_UNLOCK(&_mappingLock);

  GENERAL_SERVER_LOCK(&_commTasksLock);

  for (auto i : _commTasks) {
    i->beginShutdown();
  }

  GENERAL_SERVER_UNLOCK(&_commTasksLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

void HttpServer::stop () {
  GENERAL_SERVER_LOCK(&_commTasksLock);

  while (! _commTasks.empty()) {
    HttpCommTask* task = *_commTasks.begin();
    _commTasks.erase(task);
    GENERAL_SERVER_UNLOCK(&_commTasksLock);

    unregisterChunkedTask(task);
    _scheduler->destroyTask(task);

    GENERAL_SERVER_LOCK(&_commTasksLock);
  }

  GENERAL_SERVER_UNLOCK(&_commTasksLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleConnected (TRI_socket_t s, const ConnectionInfo& info) {
  HttpCommTask* task = createCommTask(s, info);

  GENERAL_SERVER_LOCK(&_commTasksLock);
  _commTasks.insert(task);
  GENERAL_SERVER_UNLOCK(&_commTasksLock);

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

  GENERAL_SERVER_LOCK(&_commTasksLock);
  _commTasks.erase(task);
  GENERAL_SERVER_UNLOCK(&_commTasksLock);

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleCommunicationFailure (HttpCommTask* task) {
  shutdownHandlerByTask(task);

  GENERAL_SERVER_LOCK(&_commTasksLock);
  _commTasks.erase(task);
  GENERAL_SERVER_UNLOCK(&_commTasksLock);

  unregisterChunkedTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback if the handler received a signal
////////////////////////////////////////////////////////////////////////////////

void HttpServer::handleAsync (HttpCommTask* task) {
  GENERAL_SERVER_LOCK(&_mappingLock);

  auto&& it = _task2handler.find(task);

  if (it == _task2handler.end() || it->second._task != task) {
    LOG_WARNING("cannot find a task for the handler, giving up");

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  handler_task_job_t element = it->second;
  _task2handler.erase(it);

  HttpHandler* handler = element._handler;
  _handlers.erase(handler);

  GENERAL_SERVER_UNLOCK(&_mappingLock);

  HttpResponse * response = handler->getResponse();

  if (response == nullptr) {
    basics::Exception err(TRI_ERROR_INTERNAL, 
                          "no response received from handler",
                          __FILE__, __LINE__);

    handler->handleError(err);
    response = handler->getResponse();
  }

  if (response == nullptr) {
    delete handler;
    LOG_ERROR("cannot get any response");

    return;
  }

  handler->RequestStatisticsAgent::transfer(task);

  task->handleResponse(response);
          
  delete handler;
              
  task->processRead();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequestAsync (HttpHandler* handler, uint64_t* jobId) {
  if (_dispatcher == nullptr) {
    // without a dispatcher, simply give up
    RequestStatisticsAgentSetExecuteError(handler);

    delete handler;

    LOG_WARNING("no dispatcher is known");
    return false;
  }

  // execute the handler using the dispatcher
  Job* ajob = handler->createJob(this, true);
  HttpServerJob* job = dynamic_cast<HttpServerJob*>(ajob);

  if (job == nullptr) {
    delete ajob;

    RequestStatisticsAgentSetExecuteError(handler);

    delete handler;

    LOG_WARNING("task is indirect, but handler failed to create a job - this cannot work!");
    return false;
  }

  if (jobId != nullptr) {
    try {
      _jobManager->initAsyncJob(job, jobId);
    }
    catch (...) {
      RequestStatisticsAgentSetExecuteError(handler);
      LOG_WARNING("unable to initialize job");
      delete job;
      delete handler;
      return false;
    }
  }

  int error = _dispatcher->addJob(job);
  if (error != TRI_ERROR_NO_ERROR) {
    // could not add job to job queue
    RequestStatisticsAgentSetExecuteError(handler);
    LOG_WARNING("unable to add job to the job queue: %s", TRI_errno_string(error));
    delete job;
    delete handler;
    return false;
  }

  // job is in queue now
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler directly or add it to the queue
////////////////////////////////////////////////////////////////////////////////

bool HttpServer::handleRequest (HttpCommTask* task, HttpHandler* handler) {
  registerHandler(handler, task);

  // execute handler and (possibly) requeue
  while (true) {

    // directly execute the handler within the scheduler thread
    if (handler->isDirect()) {
      Handler::status_t status = handleRequestDirectly(task, handler);

      if (status.status != Handler::HANDLER_REQUEUE) {
        shutdownHandlerByTask(task);
        return true;
      }
    }

    // execute the handler using the dispatcher
    else if (_dispatcher != nullptr) {
      Job* ajob = handler->createJob(this, false);
      HttpServerJob* job = dynamic_cast<HttpServerJob*>(ajob);

      if (job == nullptr) {
        if (ajob != nullptr) {
          delete ajob;
        }

        RequestStatisticsAgentSetExecuteError(handler);

        LOG_WARNING("task is indirect, but handler failed to create a job - this cannot work!");

        shutdownHandlerByTask(task);
        return false;
      }

      registerJob(handler, job);
      return true;
    }

    // without a dispatcher, simply give up
    else {
      RequestStatisticsAgentSetExecuteError(handler);

      LOG_WARNING("no dispatcher is known");

      shutdownHandlerByTask(task);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback if job is done
////////////////////////////////////////////////////////////////////////////////

void HttpServer::jobDone (Job* ajob) {
  HttpServerJob* job = dynamic_cast<HttpServerJob*>(ajob);

  if (job == nullptr) {
    LOG_WARNING("jobDone called, but Job is no HttpServerJob");
    return;
  }

  // locate the handler
  HttpHandler* handler = job->getHandler();

  if (job->isDetached()) {
    if (handler != nullptr) {
      _jobManager->finishAsyncJob(job);
      delete handler;
    }

    return;
  }

  GENERAL_SERVER_LOCK(&_mappingLock);
  auto&& it = _handlers.find(handler);


  if (it == _handlers.end() || it->second._handler != handler) {
    LOG_WARNING("jobDone called, but handler is unknown");

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  handler_task_job_t& element = it->second;

  // remove the job from the mapping
  element._job = nullptr;

  // if there is no task, assume the client has died
  if (element._task == nullptr) {
    LOG_DEBUG("jobDone called, but no task is known, assume client has died");
    _handlers.erase(it);

    delete handler;

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  // signal the task, to continue its work
  element._task->signal();

  GENERAL_SERVER_UNLOCK(&_mappingLock);
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
  _listenTasks.push_back(task);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

Handler::status_t HttpServer::handleRequestDirectly (HttpCommTask* task, HttpHandler * handler) {
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

    HttpResponse * response = handler->getResponse();

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
/// @brief shut downs a handler for a task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::shutdownHandlerByTask (Task* task) {
  GENERAL_SERVER_LOCK(&_mappingLock);

  // remove the task from the map
  auto&& it = _task2handler.find(task);

  if (it == _task2handler.end() || it->second._task != task) {
    LOG_DEBUG("shutdownHandler called, but no handler is known for task");

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  handler_task_job_t& element = it->second;
  HttpHandler* handler = element._handler;

  _task2handler.erase(it);

  // check if the handler contains a job or not
  auto&& jt = _handlers.find(handler);

  if (jt == _handlers.end() || jt->second._handler != handler) {
    LOG_DEBUG("shutdownHandler called, but handler of task is unknown");

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  // if we do not know a job, delete handler
  handler_task_job_t& element2 = jt->second;
  Job* job = element2._job;

  if (job == nullptr) {
    _handlers.erase(jt);

    GENERAL_SERVER_UNLOCK(&_mappingLock);

    delete handler;
    return;
  }

  // initiate shutdown if a job is known
  else {
    element2._task = nullptr;
    job->beginShutdown();

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

void HttpServer::registerHandler (HttpHandler* handler, HttpCommTask* task) {
  GENERAL_SERVER_LOCK(&_mappingLock);

  handler_task_job_t element;
  element._handler = handler;
  element._task = task;
  element._job = nullptr;

  _handlers[handler] = element;
  _task2handler[task] = element;

  GENERAL_SERVER_UNLOCK(&_mappingLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new job
////////////////////////////////////////////////////////////////////////////////

void HttpServer::registerJob (HttpHandler* handler, HttpServerJob* job) {
  GENERAL_SERVER_LOCK(&_mappingLock);

  // update the handler information
  auto&& it = _handlers.find(handler);

  if (it == _handlers.end() || it->second._handler != handler) {
    LOG_DEBUG("registerJob called for an unknown handler");

    GENERAL_SERVER_UNLOCK(&_mappingLock);
    return;
  }

  handler_task_job_t& element = it->second;
  element._job = job;

  GENERAL_SERVER_UNLOCK(&_mappingLock);

  handler->RequestStatisticsAgent::transfer(job);

  _dispatcher->addJob(job);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
