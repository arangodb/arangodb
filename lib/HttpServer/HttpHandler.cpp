////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandler.h"

#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "GeneralServer/GeneralServerJob.h"

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructs and destructors
    // -----------------------------------------------------------------------------

    HttpHandler::HttpHandler (HttpRequest* request)
      : _handlerFactory(0),
        request(request),
        response(0),
        _task(0), 
        _job(0) {
    }



    HttpHandler::~HttpHandler () {
      if (request != 0) {
        delete request;
      }

      if (response != 0) {
        delete response;
      }

      if (_task != 0) {
        _task->setHandler(0);
      }

      if (_handlerFactory != 0) {
        _handlerFactory->unregisterHandler(this);
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    HttpResponse* HttpHandler::getResponse () {
      return response;
    }



    void HttpHandler::setHandlerFactory (HttpHandlerFactory* handlerFactory) {
      _handlerFactory = handlerFactory;
    }



    bool HttpHandler::handleAsync () {
      if (_job == 0) {
        LOGGER_WARNING << "no job is known";
      }
      else {
        HttpResponse* response = getResponse();

        try {
          if (response == 0) {
            basics::InternalError err("no response received from handler");

            handleError(err);
            response = getResponse();
          }

          if (response != 0) {
            _task->handleResponse(response);
          }
        }
        catch (...) {
          LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__;
        }
 
        // this might delete the handler (i.e. ourselves!)
        return _job->beginShutdown();
      }

      return true;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create a job
    ////////////////////////////////////////////////////////////////////////////////

    Job* HttpHandler::createJob () {
      LOGGER_WARNING << "job creation requested for handler that is not intended to produce jobs";

      return 0;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create a job
    ////////////////////////////////////////////////////////////////////////////////

    Job* HttpHandler::createJob (Scheduler* scheduler, Dispatcher* dispatcher, HttpCommTask* task) {
      HttpServer* server = dynamic_cast<HttpServer*>(task->getServer());

      GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>* atask =
        dynamic_cast<GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>*>(task);

      GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>* job = 
        new GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>(server, scheduler, dispatcher, atask, this);

      setJob(job);

      return job;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief handles authentication
    ////////////////////////////////////////////////////////////////////////////////

    bool HttpHandler::handleAuthentication () {
      if (request == 0) {
        return false;
      }

      bool found;
      char const* auth = request->header("authorization", found);

      if (found) {
        return false;
      }

      response = new HttpResponse(HttpResponse::UNAUTHORIZED);

      response->setHeader("www-authenticate", "basic realm=\"arangod\"");

      return true;
    }
  }
}
