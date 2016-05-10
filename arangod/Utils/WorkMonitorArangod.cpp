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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/WorkMonitor.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include "Aql/QueryList.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "HttpServer/HttpHandler.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Task.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief pushes a handler
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::pushHandler(HttpHandler* handler) {
  TRI_ASSERT(handler != nullptr);
  WorkDescription* desc = createWorkDescription(WorkType::HANDLER);
  desc->_data.handler = handler;
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  activateWorkDescription(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops and releases a handler
////////////////////////////////////////////////////////////////////////////////

WorkDescription* WorkMonitor::popHandler(HttpHandler* handler, bool free) {
  WorkDescription* desc = deactivateWorkDescription();

  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::HANDLER);
  TRI_ASSERT(desc->_data.handler != nullptr);
  TRI_ASSERT(desc->_data.handler == handler);

  if (free) {
    try {
      freeWorkDescription(desc);
    } catch (...) {
      // just to prevent throwing exceptions from here, as this method
      // will be called in destructors...
    }
  }

  return desc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancels a query
////////////////////////////////////////////////////////////////////////////////

bool WorkMonitor::cancelAql(WorkDescription* desc) {
  auto type = desc->_type;
  
  if (type != WorkType::AQL_STRING && type != WorkType::AQL_ID) {
    return true;
  }

  TRI_vocbase_t* vocbase = desc->_vocbase;

  if (vocbase != nullptr) {
    uint64_t id = desc->_identifier._id;

    LOG(WARN) << "cancel query " << id << " in " << vocbase;

    auto queryList = static_cast<arangodb::aql::QueryList*>(vocbase->_queries);
    auto res = queryList->kill(id);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(DEBUG) << "cannot kill query " << id;
    }
  }

  desc->_canceled.store(true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handler deleter
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::deleteHandler(WorkDescription* desc) {
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  WorkItem::deleter()((WorkItem*)desc->_data.handler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread description
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::vpackHandler(VPackBuilder* b, WorkDescription* desc) {
  HttpHandler* handler = desc->_data.handler;
  const HttpRequest* request = handler->getRequest();

  b->add("type", VPackValue("http-handler"));
  b->add("protocol", VPackValue(request->protocol()));
  b->add("method",
         VPackValue(HttpRequest::translateMethod(request->requestType())));
  b->add("url", VPackValue(request->fullUrl()));
  b->add("httpVersion", VPackValue((int) request->protocolVersion()));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the overview
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::sendWorkOverview(uint64_t taskId, std::string const& data) {
  auto response = std::make_unique<HttpResponse>(GeneralResponse::ResponseCode::OK);

  response->setContentType(HttpResponse::CONTENT_TYPE_JSON);
  TRI_AppendString2StringBuffer(response->body().stringBuffer(), data.c_str(),
                                data.length());

  auto answer = std::make_unique<TaskData>();

  answer->_taskId = taskId;
  answer->_loop = SchedulerFeature::SCHEDULER->lookupLoopById(taskId);
  answer->_type = TaskData::TASK_DATA_RESPONSE;
  answer->_response.reset(response.release());

  SchedulerFeature::SCHEDULER->signalTask(answer);
}

HandlerWorkStack::HandlerWorkStack(HttpHandler* handler) : _handler(handler) {
  WorkMonitor::pushHandler(_handler);
}

HandlerWorkStack::HandlerWorkStack(WorkItem::uptr<HttpHandler>& handler) {
  _handler = handler.release();
  WorkMonitor::pushHandler(_handler);
}

HandlerWorkStack::~HandlerWorkStack() {
  WorkMonitor::popHandler(_handler, true);
}
