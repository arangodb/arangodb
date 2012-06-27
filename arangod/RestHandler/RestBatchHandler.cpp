////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/MutexLocker.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "VocBase/vocbase.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralServerBatchJob.h"
#include "GeneralServer/GeneralServerJob.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"
#include "ProtocolBuffers/HttpRequestProtobuf.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::RestBatchHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase),
    _missingResponses(0),
    _outputMessages(new PB_ArangoMessage),
    _handled(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief denstructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::~RestBatchHandler () {
  delete _outputMessages;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestBatchHandler::queue () {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestBatchHandler::execute () {
  // extract the request type
  HttpRequest::HttpRequestType type = request->requestType();
  string contentType = StringUtils::tolower(StringUtils::trim(request->header("content-type")));
  
  if (type != HttpRequest::HTTP_REQUEST_POST || contentType != "application/x-protobuf") {
    generateNotImplemented("ILLEGAL " + BATCH_PATH);
    return HANDLER_DONE;
  }

  /*
  FILE* fp = fopen("got","w");
  fwrite(request->body(), request->bodySize(), 1, fp);
  fclose(fp);
  */
  PB_ArangoMessage inputMessages;

  bool result = inputMessages.ParseFromArray(request->body(), request->bodySize());
  if (!result) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING, // TODO FIXME
                  "invalid protobuf message");
    return HANDLER_DONE;
  }

  size_t asyncResponses = 0;

  for (int i = 0; i < inputMessages.messages_size(); ++i) {
    const PB_ArangoBatchMessage inputMessage = inputMessages.messages(i);
 
    { // locked
      MUTEX_LOCKER(_handlerLock); 
      _outputMessages->add_messages();
    } // locked end

    HttpRequestProtobuf* request = new HttpRequestProtobuf(inputMessage);
    HttpHandler* handler = _task->getServer()->createHandler(request);

    if (handler == 0) {
      delete request;
      // TODO: handle fail
    }
    else {
      {
        MUTEX_LOCKER(_handlerLock);
        _handlers.push_back(handler);
      }

      if (handler->isDirect()) {
        // execute handler directly
        Handler::status_e status = Handler::HANDLER_FAILED;

        try {
          status = handler->execute();
        }
        catch (...) {
          // TODO
        }
        
        if (status != Handler::HANDLER_REQUEUE) {
          addResponse(handler);
        }
      }
      else {
        // execute handler via dispatcher
        ++_missingResponses;
        ++asyncResponses;
        HttpServer* server = dynamic_cast<HttpServer*>(_task->getServer());

        Scheduler* scheduler = server->getScheduler();
        Dispatcher* dispatcher = server->getDispatcher();

        Job* job = handler->createJob(scheduler, dispatcher, _task);
  
        GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>* generalJob = dynamic_cast<GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler> * >(job);
        generalJob->attachObserver(this);

        dispatcher->addJob(job);
      }
        
    }
  }   

  if (asyncResponses == 0) {
    // signal ourselves
    GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>* atask = 
      dynamic_cast<GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>*>(_task);

    atask->signal();
  }

  return Handler::HANDLER_DONE; 
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief create a special job for the handler
////////////////////////////////////////////////////////////////////////////////

Job* RestBatchHandler::createJob (Scheduler* scheduler, Dispatcher* dispatcher, HttpCommTask* task) {
  HttpServer* server = dynamic_cast<HttpServer*>(task->getServer());
  GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, GeneralCommTask<HttpServer, HttpHandlerFactory> >* atask = 
    dynamic_cast<GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, GeneralCommTask<HttpServer, HttpHandlerFactory> >*>(task);

  GeneralServerBatchJob<HttpServer, HttpHandlerFactory::GeneralHandler>* job
    = new GeneralServerBatchJob<HttpServer, HttpHandlerFactory::GeneralHandler>(server, scheduler, dispatcher, atask, this);

  setJob(job);

  return job;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a response supplied by a handler
////////////////////////////////////////////////////////////////////////////////
  
void RestBatchHandler::addResponse (HttpHandler* handler) {
  MUTEX_LOCKER(_handlerLock);

  for (size_t i = 0; i < _handlers.size(); ++i) {
    if (_handlers[i] == handler) {
      PB_ArangoBatchMessage* batch = _outputMessages->mutable_messages(i);

      handler->getResponse()->write(batch);
      return;
    }
  }
  
  // handler not found
  LOGGER_WARNING << "handler not found. this should not happen."; 
}
    

////////////////////////////////////////////////////////////////////////////////
/// @brief notification routine
////////////////////////////////////////////////////////////////////////////////

void RestBatchHandler::notify (Job* job, const Job::notification_e type) {
  if (type != Job::JOB_CLEANUP) {
    return;
  }
        
  GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>* generalJob = dynamic_cast<GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler> * >(job);

  HttpHandler* handler = generalJob->getHandler();
  addResponse(handler);
  if (--_missingResponses == 0) {
    handleAsync();
  }
}


bool RestBatchHandler::handleAsync () {
  if (_missingResponses == 0) {
    { // locked
      MUTEX_LOCKER(_handlerLock);
      if (_handled) {
        return true;
      }

      _handled = true;
    } // lock end
    
    assembleResponse();
  
    GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>* generalJob = dynamic_cast<GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler> * >(_job);
    generalJob->setDone();

    _task->setHandler(0);
    GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>* atask = 
      dynamic_cast<GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>*>(_task);
    atask->setHandler(0);
//    atask->signal();

    _task->handleResponse(getResponse());
    return _job->beginShutdown();
  }

  return true;
}


void RestBatchHandler::assembleResponse () {
  assert(_missingResponses == 0);

  response = new HttpResponse;
  response->setContentType("application/x-protobuf");

  string data;
  if (!_outputMessages->SerializeToString(&data)) {
    // TODO
  }
  response->body().appendText(data);
  
  MUTEX_LOCKER(_handlerLock); // locked
  for (size_t i = 0; i < _handlers.size(); ++i) {
    HttpHandler* handler = _handlers[i];
    _task->getServer()->destroyHandler(handler);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
