////////////////////////////////////////////////////////////////////////////////
/// @brief work monitor class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/WorkMonitor.h"

#include <iostream>

#include "HttpServer/HttpHandler.h"
#include "velocypack/Builder.h"
#include "velocypack/Value.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                            class HandlerWorkStack
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

HandlerWorkStack::HandlerWorkStack (HttpHandler* handler) 
  : _handler(handler) {
  WorkMonitor::pushHandler(_handler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

HandlerWorkStack::HandlerWorkStack (WorkItem::uptr<HttpHandler>& handler) {
  _handler = handler.release();
  WorkMonitor::pushHandler(_handler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

HandlerWorkStack::~HandlerWorkStack () {
  WorkMonitor::popHandler(_handler, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class WorkMonitor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a handler
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::pushHandler (HttpHandler* handler) {
  TRI_ASSERT(handler != nullptr);
  WorkDescription* desc = createWorkDescription(WorkType::HANDLER);
  desc->_data.handler = handler;
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  activateWorkDescription(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops and releases a handler
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::popHandler (HttpHandler* handler, bool free) {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::HANDLER);
  TRI_ASSERT(desc->_data.handler != nullptr);
  TRI_ASSERT(desc->_data.handler == handler);

  if (free) {
    try {
      freeWorkDescription(desc);
    }
    catch (...) {
      // just to prevent throwing exceptions from here, as this method
      // will be called in destructors...
    }
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread deleter
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::DELETE_HANDLER (WorkDescription* desc) {
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  WorkItem::deleter()((WorkItem*) desc->_data.handler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread description string
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::VPACK_HANDLER (VPackBuilder* b, WorkDescription* desc) {
  HttpHandler* handler = desc->_data.handler;
  const HttpRequest* request = handler->getRequest();

  b->add("type", VPackValue("http-handler"));
  b->add("protocol", VPackValue(request->protocol()));
  b->add("method", VPackValue(HttpRequest::translateMethod(request->requestType())));
  b->add("url", VPackValue(request->fullUrl()));
  b->add("httpVersion", VPackValue(request->httpVersion()));
  b->add("database", VPackValue(request->databaseName()));
  b->add("user", VPackValue(request->user()));
  b->add("taskId", VPackValue(request->clientTaskId()));

  if (handler->_statistics != nullptr) {
    b->add("startTime", VPackValue(handler->_statistics->_requestStart));
  }

  auto& info = request->connectionInfo();

  b->add("server", VPackValue(VPackValueType::Object));
  b->add("address", VPackValue(info.serverAddress));
  b->add("port", VPackValue(info.serverPort));
  b->close();

  b->add("client", VPackValue(VPackValueType::Object));
  b->add("address", VPackValue(info.clientAddress));
  b->add("port", VPackValue(info.clientPort));
  b->close();

  b->add("endpoint", VPackValue(VPackValueType::Object));
  b->add("address", VPackValue(info.endpoint));
  b->add("type", VPackValue(info.portType()));
  b->close();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
