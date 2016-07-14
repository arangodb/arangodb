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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTP_SERVER_H
#define ARANGOD_HTTP_SERVER_HTTP_SERVER_H 1

#include "Scheduler/TaskManager.h"

#include "Basics/Mutex.h"
#include "Endpoint/ConnectionInfo.h"
#include "HttpServer/RestHandler.h"

namespace arangodb {
class EndpointList;

namespace rest {

class AsyncJobManager;
class Dispatcher;
class HttpCommTask;
class HttpServerJob;
class Job;
class ListenTask;
class RestHandlerFactory;

class HttpServer : protected TaskManager {
  HttpServer(HttpServer const&) = delete;
  HttpServer const& operator=(HttpServer const&) = delete;

 public:
  // destroys an endpoint server
  static int sendChunk(uint64_t, std::string const&);

 public:
  HttpServer(double keepAliveTimeout,
             bool allowMethodOverride, 
             std::vector<std::string> const& accessControlAllowOrigins);
  virtual ~HttpServer();

 public:
  // returns the protocol
  virtual char const* protocol() const { return "http"; }

  // returns the encryption to be used
  virtual Endpoint::EncryptionType encryptionType() const {
    return Endpoint::EncryptionType::NONE;
  }

  // check, if we allow a method override
  bool allowMethodOverride() {
    return _allowMethodOverride;
  }

  // generates a suitable communication task
  virtual HttpCommTask* createCommTask(TRI_socket_t, ConnectionInfo&&);

 public:
  // list of trusted origin urls for CORS 
  std::vector<std::string> const& trustedOrigins() const { 
    return _accessControlAllowOrigins;
  }

  // adds the endpoint list
  void setEndpointList(const EndpointList* list);

  // starts listening
  void startListening();

  // stops listining
  void stopListening();

  // removes all listen and comm tasks
  void stop();

  // handles connection request
  void handleConnected(TRI_socket_t s, ConnectionInfo&& info);

  // handles a connection close
  void handleCommunicationClosed(HttpCommTask*);

  // handles a connection failure
  void handleCommunicationFailure(HttpCommTask*);

  // creates a job for asynchronous execution
  bool handleRequestAsync(HttpCommTask*, arangodb::WorkItem::uptr<RestHandler>&,
                          uint64_t* jobId);

  // executes the handler directly or add it to the queue
  bool handleRequest(HttpCommTask*, arangodb::WorkItem::uptr<RestHandler>&);

 protected:
  // Handler, Job, and Task tuple
  struct handler_task_job_t {
    RestHandler* _handler;
    HttpCommTask* _task;
    HttpServerJob* _job;
  };

 protected:
  // opens a listen port
  bool openEndpoint(Endpoint* endpoint);

  // handle request directly
  void handleRequestDirectly(RestHandler* handler, HttpCommTask* task);

  // registers a task
  void registerHandler(RestHandler* handler, HttpCommTask* task);

 protected:
  // active listen tasks
  std::vector<ListenTask*> _listenTasks;

  // defined ports and addresses
  const EndpointList* _endpointList;

  // mutex for comm tasks
  arangodb::Mutex _commTasksLock;

  // active comm tasks
  std::unordered_set<HttpCommTask*> _commTasks;

  // keep-alive timeout
  double _keepAliveTimeout;

  // allow to override the method
  bool _allowMethodOverride;
  
  // list of trusted origin urls for CORS 
  std::vector<std::string> const _accessControlAllowOrigins;
};
}
}

#endif
