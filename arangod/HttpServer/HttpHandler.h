////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for http handlers
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_HTTP_HANDLER_H
#define ARANGODB_HTTP_SERVER_HTTP_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Dispatcher/Job.h"
#include "Rest/HttpResponse.h"
#include "Statistics/StatisticsAgent.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class Dispatcher;
    class HttpHandlerFactory;
    class HttpRequest;
    class HttpServer;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class HttpHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for http handlers
////////////////////////////////////////////////////////////////////////////////

    class HttpHandler : public RequestStatisticsAgent {
      HttpHandler (HttpHandler const&) = delete;
      HttpHandler& operator= (HttpHandler const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
///
/// Note that the handler owns the request and the response. It is its
/// responsibility to destroy them both. See also the two steal methods.
////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpHandler (HttpRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler
////////////////////////////////////////////////////////////////////////////////

        virtual ~HttpHandler ();

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief status of execution
////////////////////////////////////////////////////////////////////////////////

        enum status_e {
          HANDLER_DONE,
          HANDLER_REQUEUE,
          HANDLER_FAILED
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief result of execution
////////////////////////////////////////////////////////////////////////////////

        class status_t {
          public:
            status_t ()
              : status_t(HANDLER_FAILED) {
            }

            explicit
            status_t (status_e status)
              : status(status),
                sleep(0.0) {
            }

            Job::status_t jobStatus () {
              switch (status) {
                case HttpHandler::HANDLER_DONE:
                  return Job::status_t(Job::JOB_DONE);

                case HttpHandler::HANDLER_REQUEUE: {
                  Job::status_t result(Job::JOB_REQUEUE);
                  result.sleep = sleep;
                  return result;
                }

                case HttpHandler::HANDLER_FAILED:
                default:
                  return Job::status_t(Job::JOB_FAILED);
              }
            }

            status_e status;
            double sleep;
        };

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a handler is executed directly
////////////////////////////////////////////////////////////////////////////////

        virtual bool isDirect () const = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name
////////////////////////////////////////////////////////////////////////////////

        virtual size_t queue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the thread which currently dealing with the job
////////////////////////////////////////////////////////////////////////////////

        virtual void setDispatcherThread (DispatcherThread*);

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares execution of a handler, has to be called before execute
////////////////////////////////////////////////////////////////////////////////

        virtual void prepareExecute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a handler
////////////////////////////////////////////////////////////////////////////////

        virtual status_t execute () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief finalizes execution of a handler, has to be called after execute
////////////////////////////////////////////////////////////////////////////////

        virtual void finalizeExecute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel an execution
////////////////////////////////////////////////////////////////////////////////

        virtual bool cancel ();

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error
////////////////////////////////////////////////////////////////////////////////

        virtual void handleError (basics::Exception const&) = 0;


////////////////////////////////////////////////////////////////////////////////
/// @brief adds a response
////////////////////////////////////////////////////////////////////////////////

        virtual void addResponse (HttpHandler*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief register the server object
////////////////////////////////////////////////////////////////////////////////

        void setServer (HttpHandlerFactory* server);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the request
////////////////////////////////////////////////////////////////////////////////

        HttpRequest const* getRequest () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the pointer to the request
////////////////////////////////////////////////////////////////////////////////

        HttpRequest* stealRequest ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* getResponse () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the response
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* stealResponse ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the handler has only one response, otherwise we'd have a leak
////////////////////////////////////////////////////////////////////////////////

        void removePreviousResponse ();

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new HTTP response
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* createResponse (HttpResponse::HttpResponseCode);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief current dispatcher thread
////////////////////////////////////////////////////////////////////////////////

        DispatcherThread* _dispatcherThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request
////////////////////////////////////////////////////////////////////////////////

        HttpRequest* _request;

////////////////////////////////////////////////////////////////////////////////
/// @brief the response
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* _response;

////////////////////////////////////////////////////////////////////////////////
/// @brief the server
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory* _server;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
