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
#include "Basics/ConditionLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Task.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::rest;

void WorkMonitor::run() {
  CONDITION_LOCKER(guard, _waiter);

  uint32_t const maxSleep = 100 * 1000;
  uint32_t const minSleep = 100;
  uint32_t s = minSleep;

  // clean old entries and create summary if requested
  while (!isStopping()) {
    try {
      bool found = false;
      WorkDescription* desc;

      while (freeableWorkDescription.pop(desc)) {
        found = true;
        if (desc != nullptr) {
          deleteWorkDescription(desc, false);
        }
      }

      if (found) {
        s = minSleep;
      } else if (s < maxSleep) {
        s *= 2;
      }

      {
        MUTEX_LOCKER(guard, cancelLock);

        if (!cancelIds.empty()) {
          for (auto thread : threads) {
            cancelWorkDescriptions(thread);
          }

          cancelIds.clear();
        }
      }

      rest::RestHandler* handler;

      while (workOverview.pop(handler)) {
        VPackBuilder builder;

        builder.add(VPackValue(VPackValueType::Object));

        builder.add("time", VPackValue(TRI_microtime()));
        builder.add("work", VPackValue(VPackValueType::Array));

        {
          MUTEX_LOCKER(guard, threadsLock);

          for (auto& thread : threads) {
            WorkDescription* desc = thread->workDescription();

            if (desc != nullptr) {
              builder.add(VPackValue(VPackValueType::Object));
              vpackWorkDescription(&builder, desc);
              builder.close();
            }
          }
        }

        builder.close();
        builder.close();

        sendWorkOverview(handler, builder.steal());
      }
    } catch (...) {
      // must prevent propagation of exceptions from here
    }

    guard.wait(s);
  }

  // indicate that we stopped the work monitor, freeWorkDescription
  // should directly delete old entries
  stopped.store(true);

  // cleanup old entries
  WorkDescription* desc;

  while (freeableWorkDescription.pop(desc)) {
    if (desc != nullptr) {
      deleteWorkDescription(desc, false);
    }
  }

  while (emptyWorkDescription.pop(desc)) {
    if (desc != nullptr) {
      delete desc;
    }
  }
}

void WorkMonitor::pushHandler(RestHandler* handler) {
  TRI_ASSERT(handler != nullptr);
  WorkDescription* desc = createWorkDescription(WorkType::HANDLER);
  desc->_data.handler = handler;
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  activateWorkDescription(desc);
}

WorkDescription* WorkMonitor::popHandler(RestHandler* handler, bool free) {
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

bool WorkMonitor::cancelAql(WorkDescription* desc) {
  auto type = desc->_type;

  if (type != WorkType::AQL_STRING && type != WorkType::AQL_ID) {
    return true;
  }

  TRI_vocbase_t* vocbase = desc->_vocbase;

  if (vocbase != nullptr) {
    uint64_t id = desc->_identifier._id;

    LOG(WARN) << "cancel query " << id << " in " << vocbase;

    auto queryList = vocbase->queryList();
    auto res = queryList->kill(id);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(DEBUG) << "cannot kill query " << id;
    }
  }

  desc->_canceled.store(true);

  return true;
}

void WorkMonitor::deleteHandler(WorkDescription* desc) {
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  WorkItem::deleter()((WorkItem*)desc->_data.handler);
}

void WorkMonitor::vpackHandler(VPackBuilder* b, WorkDescription* desc) {
  RestHandler* handler = desc->_data.handler;
  GeneralRequest const* request = handler->request();

  b->add("type", VPackValue("http-handler"));
  b->add("protocol", VPackValue(request->protocol()));
  b->add("method",
         VPackValue(HttpRequest::translateMethod(request->requestType())));
  b->add("url", VPackValue(request->fullUrl()));
  b->add("httpVersion", VPackValue((int)request->protocolVersion()));
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

void WorkMonitor::sendWorkOverview(
    RestHandler* handler, std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) {
  auto response = handler->response();

  velocypack::Slice slice(buffer->data());
  response->setResponseCode(rest::ResponseCode::OK);
  response->setPayload(slice, true, VPackOptions::Defaults);

  auto data = std::make_unique<TaskData>();

  data->_taskId = handler->taskId();
  data->_loop = handler->eventLoop();
  data->_type = TaskData::TASK_DATA_RESPONSE;
  data->_response = handler->stealResponse();

  SchedulerFeature::SCHEDULER->signalTask(data);

  delete static_cast<WorkItem*>(handler);
}

HandlerWorkStack::HandlerWorkStack(RestHandler* handler) : _handler(handler) {
  WorkMonitor::pushHandler(_handler);
}

HandlerWorkStack::HandlerWorkStack(WorkItem::uptr<RestHandler> handler) {
  _handler = handler.release();
  WorkMonitor::pushHandler(_handler);
}

HandlerWorkStack::~HandlerWorkStack() {
  WorkMonitor::popHandler(_handler, true);
}
