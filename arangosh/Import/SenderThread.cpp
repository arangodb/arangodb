////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "SenderThread.h"
#include "Basics/Common.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ImportHelper.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::import;

SenderThread::SenderThread(
    std::unique_ptr<httpclient::SimpleHttpClient>&& client,
    ImportStatistics* stats)
    : Thread("Import Sender"),
      _client(client.release()),
      _data(TRI_UNKNOWN_MEM_ZONE, false),
      _hasError(false),
      _idle(true),
      _ready(false),
      _stats(stats) {}

SenderThread::~SenderThread() {
  shutdown();
  delete _client;
}

void SenderThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void SenderThread::sendData(std::string const& url,
                            arangodb::basics::StringBuffer* data) {
  TRI_ASSERT(_idle && !_hasError);
  _url = url;
  _data.swap(data);

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  _idle = false;
  guard.broadcast();
}

bool SenderThread::hasError() {
  CONDITION_LOCKER(guard, _condition);
  return _hasError;
}

bool SenderThread::isReady() {
  CONDITION_LOCKER(guard, _condition);
  return _ready;
}

bool SenderThread::isIdle() {
  CONDITION_LOCKER(guard, _condition);
  return _idle;
}

bool SenderThread::isDone() {
  CONDITION_LOCKER(guard, _condition);
  return _idle || _hasError;
}

void SenderThread::run() {
  while (!isStopping() && !_hasError) {
    {
      CONDITION_LOCKER(guard, _condition);
      _ready = true;
      if (_idle) {
        guard.wait();
      }
    }
    if (isStopping()) {
      break;
    }
    try {
      if (_data.length() > 0) {
        TRI_ASSERT(!_idle && !_url.empty());

        std::unique_ptr<httpclient::SimpleHttpResult> result(
            _client->request(rest::RequestType::POST, _url, _data.c_str(),
                             _data.length()));

        handleResult(result.get());

        _url.clear();
        _data.reset();
      }

      CONDITION_LOCKER(guard, _condition);
      _idle = true;
    } catch (...) {
      CONDITION_LOCKER(guard, _condition);
      _hasError = true;
      _idle = true;
    }
  }
    
  CONDITION_LOCKER(guard, _condition);
  TRI_ASSERT(_idle);
}

void SenderThread::handleResult(httpclient::SimpleHttpResult* result) {
  if (result == nullptr) {
    return;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = result->getBodyVelocyPack();
  } catch (...) {
    // No action required
    return;
  }
  VPackSlice const body = parsedBody->slice();

  // error details
  VPackSlice const details = body.get("details");

  if (details.isArray()) {
    for (VPackSlice const& detail : VPackArrayIterator(details)) {
      if (detail.isString()) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "" << detail.copyString();
      }
    }
  }
  
  {
    // first update all the statistics
    MUTEX_LOCKER(guard, _stats->_mutex);
    // look up the "created" flag
    _stats->_numberCreated +=
    arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(body,
                                                                "created", 0);
    
    // look up the "errors" flag
    _stats->_numberErrors +=
    arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(body,
                                                                "errors", 0);
    
    // look up the "updated" flag
    _stats->_numberUpdated +=
    arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(body,
                                                                "updated", 0);
    
    // look up the "ignored" flag
    _stats->_numberIgnored +=
    arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(body,
                                                                "ignored", 0);
  }

  // get the "error" flag. This returns a pointer, not a copy
  if (arangodb::basics::VelocyPackHelper::getBooleanValue(body, "error",
                                                          false)) {

    // get the error message
    VPackSlice const errorMessage = body.get("errorMessage");
    if (errorMessage.isString()) {
      _errorMessage = errorMessage.copyString();
    }
    
    // will trigger the waiting ImportHelper thread to cancel the import
    _hasError = true;
  }
}
