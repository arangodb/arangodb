////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

SenderThread::SenderThread(application_features::ApplicationServer& server,
                           std::unique_ptr<httpclient::SimpleHttpClient> client,
                           ImportStatistics* stats, std::function<void()> const& wakeup)
    : Thread(server, "Import Sender"),
      _client(std::move(client)),
      _wakeup(wakeup),
      _data(false),
      _hasError(false),
      _idle(true),
      _ready(false),
      _lowLineNumber(0),
      _highLineNumber(0),
      _stats(stats) {}

SenderThread::~SenderThread() { shutdown(); }

void SenderThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void SenderThread::sendData(std::string const& url, arangodb::basics::StringBuffer* data,
                            size_t lowLine, size_t highLine) {
  TRI_ASSERT(_idle && !_hasError);
  _url = url;
  _data.swap(data);

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  _idle = false;
  _lowLineNumber = lowLine;
  _highLineNumber = highLine;
  guard.broadcast();
}

bool SenderThread::hasError() {
  bool retFlag = false;
  {
    // flag reset after read to prevent multiple reporting
    //  of errors in ImportHelper
    CONDITION_LOCKER(guard, _condition);
    retFlag = _hasError;
    _hasError = false;
  }

  if (retFlag) {
    beginShutdown();
  }
  return retFlag;
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

        {
          QuickHistogramTimer timer(_stats->_histogram, (_highLineNumber - _lowLineNumber) +1);
          std::unique_ptr<httpclient::SimpleHttpResult> result(
              _client->request(rest::RequestType::POST, _url, _data.c_str(),
                               _data.length()));

          handleResult(result.get());
        }

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

    _wakeup();
  }

  CONDITION_LOCKER(guard, _condition);
  TRI_ASSERT(_idle);
}

void SenderThread::handleResult(httpclient::SimpleHttpResult* result) {
  bool haveBody = false;

  if (result == nullptr) {
    return;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = result->getBodyVelocyPack();
    haveBody = true;
  } catch (...) {
    // no body, likely error situation
  }

  if (haveBody) {
    VPackSlice const body = parsedBody->slice();

    // error details
    VPackSlice const details = body.get("details");

    if (details.isArray()) {
      for (VPackSlice detail : VPackArrayIterator(details)) {
        if (detail.isString()) {
          LOG_TOPIC("e5a29", WARN, arangodb::Logger::FIXME) << "" << detail.copyString();
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
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(body, "error", false)) {
      // get the error message
      VPackSlice const errorMessage = body.get("errorMessage");
      if (errorMessage.isString()) {
        _errorMessage = errorMessage.copyString();
      }

      // will trigger the waiting ImportHelper thread to cancel the import
      _hasError = true;
    }
  } // if

  if (!_hasError && !result->getHttpReturnMessage().empty() && !result->isComplete()) {
    _errorMessage = result->getHttpReturnMessage();
    if (0 != _lowLineNumber || 0 != _highLineNumber) {
      LOG_TOPIC("8add8", WARN, arangodb::Logger::FIXME) << "Error left import lines "
                                                        << _lowLineNumber << " through "
                                                        << _highLineNumber
                                                        << " in unknown state";
    } // if
    _hasError = true;
  } // if
}
