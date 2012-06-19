////////////////////////////////////////////////////////////////////////////////
/// @brief ZeroMQ batch job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ZeroMQBatchJob.h"

#include "Basics/Exceptions.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProtocolBuffers/HttpRequestProtobuf.h"
#include "Rest/HttpResponse.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructrs a new jobs
////////////////////////////////////////////////////////////////////////////////

ZeroMQBatchJob::ZeroMQBatchJob (zframe_t* address,
                                HttpHandlerFactory* handlerFactory,
                                char const* data,
                                size_t length) 
  : Job("ZeroMQBatchJob"),
    _requests(),
    _responses(),
    _address(address),
    _handlerFactory(handlerFactory),
    _handler(0),
    _nrCurrentRequest(0),
    _request(0) {
  int ok = _requests.ParseFromArray(data, length);

  if (! ok) {
    PB_ArangoBatchMessage* message = _responses.add_messages();

    LOGGER_ERROR << "received corrupted ZeroMQ message in " << __FILE__ << "@" << __LINE__;

    message->set_type(PB_ERROR_RESPONSE);

    PB_ArangoErrorResponse* em = message->mutable_errorresponse();
    em->set_message("cannot get any response from handler");

    _nrCurrentRequest = (size_t) _requests.messages().size();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructors
////////////////////////////////////////////////////////////////////////////////

ZeroMQBatchJob::~ZeroMQBatchJob () {

  // clear the current handler
  if (_handler != 0) {
    delete _handler;
    _handler = 0;
  }

  // clear the current request
  if (_request != 0) {
    delete _request;
    _request = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if all requests are handled
////////////////////////////////////////////////////////////////////////////////

bool ZeroMQBatchJob::isDone () {
  return ! (_nrCurrentRequest < (size_t) _requests.messages().size());

}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if current request is direct
////////////////////////////////////////////////////////////////////////////////

bool ZeroMQBatchJob::isDirect () {
  return (_handler != 0) ? _handler->isDirect() : true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::JobType ZeroMQBatchJob::type ()  {
  return _handler == 0 ? Job::READ_JOB : _handler->type();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& ZeroMQBatchJob::queue () {
  static string const standard = "STANDARD";
  return _handler == 0 ? standard : _handler->queue();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::setDispatcherThread (DispatcherThread* thread) {
  if (_handler != 0) {
    _handler->setDispatcherThread(thread);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_e ZeroMQBatchJob::work () {
  Handler::status_e status = executeCurrentRequest();

  // if the handler wants to requeue, stop and put job back onto the queue
  if (status == Handler::HANDLER_REQUEUE) {
    return JOB_REQUEUE;
  }

  // if the handler failed, skip this request
  if (status == Handler::HANDLER_FAILED) {
    handleInternalError();
  }

  // if we dealt with all requests, send response back to client
  if (isDone()) {
    return JOB_DONE_ZEROMQ;
  }

  // handle direct requests now
  if (isDirect()) {
    return work();
  }

  // otherwise put job back onto the queue, handle next request
  return JOB_REQUEUE;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::cleanup () {
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
///
/// Finishes the job and sends the result to the internal ZeroMQ server.
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::finish (void* bridge) {

  // need a bridge the ZeroMQ queue
  if (bridge != 0) {

    // serialize protocol buffers
    string data;
    _responses.SerializeToString(&data);

    // copy serialized data into message
    zframe_t* empty = zframe_new("", 0);
    zframe_t* content = zframe_new(data.c_str(), data.size());

    // send the peer address, followed by the content
    zframe_send(&_address, bridge, ZFRAME_MORE);
    zframe_send(&empty, bridge, ZFRAME_MORE);
    zframe_send(&content, bridge, 0);
  }

  // not good
  else {
    LOGGER_ERROR << "cannot communicate with internal ZeroMQ queue, no socket is known";
  }

  // commit suicide
  cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::handleError (TriagensError const& ex) {
  _handler->handleError(ex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the current request
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::extractNextRequest () {

  // clear the current handler
  if (_handler != 0) {
    delete _handler;
    _handler = 0;
  }

  // clear the current request
  if (_request != 0) {
    delete _request;
    _request = 0;
  }

  if (_nrCurrentRequest < (size_t) _requests.messages().size()) {
    _request = new HttpRequestProtobuf(_requests.messages().Get(_nrCurrentRequest));

    if (_handlerFactory != 0) {
      _handler = _handlerFactory->createHandler(_request);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the current request
////////////////////////////////////////////////////////////////////////////////

Handler::status_e ZeroMQBatchJob::executeCurrentRequest () {
  Handler::status_e status = Handler::HANDLER_FAILED;

  try {
    if (_handler == 0) {
      handleNotFound();
      status = Handler::HANDLER_DONE;
    }
    else {
      try {
        status = _handler->execute();
      }
      catch (basics::TriagensError const& ex) {
        _handler->handleError(ex);
      }
      catch (std::exception const& ex) {
        basics::InternalError err(ex);

        _handler->handleError(err);
      }
      catch (...) {
        basics::InternalError err;
        _handler->handleError(err);
      }

      if (status == Handler::HANDLER_REQUEUE) {
        return status;
      }

      // store a response or an error message
      handleResponse();
    }

    // get next request and request handler
    ++_nrCurrentRequest;
    extractNextRequest();
  }
  catch (basics::TriagensError const& ex) {
    LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__ << ": " << DIAGNOSTIC_INFORMATION(ex);
  }
  catch (std::exception const& ex) {
    LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__ << ": " << ex.what();
  }
  catch (...) {
    LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__;
  }

  return status;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles one response
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::handleResponse () {
  HttpResponse* response = _handler->getResponse();

  // we need a response, otherwise create an error object
  if (response == 0) {
    InternalError err("no response received from handler");

    _handler->handleError(err);
    response = _handler->getResponse();
  }

  // send the response to ZeroMQ
  PB_ArangoBatchMessage* message = _responses.add_messages();

  if (response != 0) {
    response->write(message);
  }
  else {
    LOGGER_ERROR << "cannot get any response in " << __FILE__ << "@" << __LINE__;

    message->set_type(PB_ERROR_RESPONSE);

    PB_ArangoErrorResponse* em = message->mutable_errorresponse();
    em->set_message("cannot get any response from handler");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles not-found
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::handleNotFound () {
  PB_ArangoBatchMessage* message = _responses.add_messages();

  message->set_type(PB_BLOB_RESPONSE);

  PB_ArangoBlobResponse* response = message->mutable_blobresponse();

  response->set_status(HttpResponse::NOT_FOUND);
  response->set_contenttype(PB_NO_CONTENT);
  response->set_contentlength(0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles server error
////////////////////////////////////////////////////////////////////////////////

void ZeroMQBatchJob::handleInternalError () {
  HttpResponse* response = _handler->getResponse();
  PB_ArangoBatchMessage* message = _responses.add_messages();

  if (response == 0) {
    message->set_type(PB_BLOB_RESPONSE);

    PB_ArangoBlobResponse* response = message->mutable_blobresponse();

    response->set_status(HttpResponse::SERVER_ERROR);
    response->set_contenttype(PB_NO_CONTENT);
  }
  else {
    response->write(message);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
