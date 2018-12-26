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
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                       WorkMonitor
// -----------------------------------------------------------------------------

bool WorkMonitor::clearWorkDescriptions() {
  bool found = false;
  WorkDescription* desc;

  // handle freeable work descriptions
  while (_freeableWorkDescription.pop(desc)) {
    found = true;

    if (desc != nullptr) {
      deleteWorkDescription(desc, false);
      found = true;
      desc = nullptr;
    }
  }

  return found;
}

void WorkMonitor::run() {
  CONDITION_LOCKER(guard, _waiter);

  uint32_t const maxSleep = 100 * 1000;
  uint32_t const minSleep = 100;
  uint32_t s = minSleep;

  // clean old entries and create summary if requested
  while (!isStopping()) {
    try {
      bool found = clearWorkDescriptions();

      if (found) {
        s = minSleep;
      } else if (s < maxSleep) {
        s *= 2;
      }

      // handle cancel requests
      {
        MUTEX_LOCKER(guard, _cancelLock);

        if (!_cancelIds.empty()) {
          for (auto thread : _threads) {
            cancelWorkDescriptions(thread);
          }

          _cancelIds.clear();
        }
      }

      // handle work descriptions requests -- hac - handler and callback
      std::pair<std::shared_ptr<rest::RestHandler>, std::function<void()>>* hac;

      while (_workOverview.pop(hac)) {
        VPackBuilder builder;

        builder.add(VPackValue(VPackValueType::Object));

        builder.add("time", VPackValue(TRI_microtime()));
        builder.add("work", VPackValue(VPackValueType::Array));

        {
          MUTEX_LOCKER(guard, _threadsLock);

          for (auto& thread : _threads) {
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

        addWorkOverview(hac->first, builder.steal());
        hac->second();  // callback
        delete hac;
      }
    } catch (...) {
      // must prevent propagation of exceptions from here
    }

    guard.wait(s);
  }

  // indicate that we stopped the work monitor, freeWorkDescription
  // should directly delete old entries
  _stopped.store(true);

  // cleanup old entries
  clearWorkDescriptions();

  WorkDescription* desc;
  while (_emptyWorkDescription.pop(desc)) {
    if (desc != nullptr) {
      delete desc;
    }
  }

  clearAllHandlers();
}

void WorkMonitor::clearAllHandlers() {
  std::shared_ptr<rest::RestHandler>* shared;

  while (_workOverview.pop(shared)) {
    delete shared;
  }

  _waiter.broadcast();
}

void WorkMonitor::pushHandler(std::shared_ptr<RestHandler> handler) {
  WorkDescription* desc = createWorkDescription(WorkType::HANDLER);
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  desc->_context = handler->context();

  new (&desc->_data._handler._handler) std::shared_ptr<RestHandler>(handler);
  new (&desc->_data._handler._canceled) std::atomic<bool>(false);

  activateWorkDescription(desc);
  RestHandler::CURRENT_HANDLER = handler.get();
}

void WorkMonitor::popHandler() {
  WorkDescription* desc = deactivateWorkDescription();
  TRI_ASSERT(desc != nullptr);
  TRI_ASSERT(desc->_type == WorkType::HANDLER);
  TRI_ASSERT(desc->_data._handler._handler != nullptr);

  try {
    freeWorkDescription(desc);
  } catch (...) {
    // just to prevent throwing exceptions from here, as this method
    // will be called in destructors...
  }

  // TODO(fc) we might have a stack of handlers
  RestHandler::CURRENT_HANDLER = nullptr;
}

bool WorkMonitor::cancelAql(WorkDescription* desc) {
  auto type = desc->_type;

  if (type != WorkType::AQL_STRING && type != WorkType::AQL_ID) {
    return true;
  }

  TRI_vocbase_t* vocbase = desc->_data._aql._vocbase;

  if (vocbase != nullptr) {
    uint64_t id = desc->_data._aql._id;

    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "cancel query " << id << " in " << vocbase;

    auto queryList = vocbase->queryList();
    auto res = queryList->kill(id);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot kill query " << id;
    }
  }

  desc->_data._aql._canceled.store(true);

  return true;
}

void WorkMonitor::deleteHandler(WorkDescription* desc) {
  TRI_ASSERT(desc->_type == WorkType::HANDLER);

  desc->_data._handler._handler.std::shared_ptr<rest::RestHandler>::~shared_ptr();

  desc->_data._handler._canceled.std::atomic<bool>::~atomic();
}

void WorkMonitor::vpackHandler(VPackBuilder* b, WorkDescription* desc) {
  RestHandler* handler = desc->_data._handler._handler.get();
  GeneralRequest const* request = handler->request();

  b->add("type", VPackValue("http-handler"));
  b->add("protocol", VPackValue(request->protocol()));
  b->add("method", VPackValue(HttpRequest::translateMethod(request->requestType())));
  b->add("url", VPackValue(request->fullUrl()));
  b->add("httpVersion", VPackValue((int)request->protocolVersion()));
  b->add("database", VPackValue(request->databaseName()));
  b->add("user", VPackValue(request->user()));
  b->add("taskId", VPackValue(request->clientTaskId()));

  auto statistics = handler->statistics();

  if (statistics != nullptr) {
    b->add("startTime", VPackValue(statistics->requestStart()));
  } else {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "missing statistics";
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

void WorkMonitor::addWorkOverview(std::shared_ptr<RestHandler> handler,
                                  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) {
  auto response = handler->response();

  velocypack::Slice slice(buffer->data());
  response->setResponseCode(rest::ResponseCode::OK);
  response->setPayload(slice, true, VPackOptions::Defaults);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  HandlerWorkStack
// -----------------------------------------------------------------------------

HandlerWorkStack::HandlerWorkStack(std::shared_ptr<RestHandler> handler)
    : _handler(handler) {
  WorkMonitor::pushHandler(_handler);
}

HandlerWorkStack::~HandlerWorkStack() { WorkMonitor::popHandler(); }
