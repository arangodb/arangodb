////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ImportHelper.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::import;

SenderThread::SenderThread(application_features::ApplicationServer& server,
                           std::unique_ptr<httpclient::SimpleHttpClient> client,
                           ImportStatistics* stats,
                           std::function<void()> const& wakeup)
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
  std::lock_guard guard{_condition.mutex};
  _condition.cv.notify_all();
}

void SenderThread::sendData(std::string const& url,
                            arangodb::basics::StringBuffer* data,
                            size_t lowLine, size_t highLine) {
  TRI_ASSERT(_idle && !_hasError);
  _url = url;
  _data.swap(data);

  // wake up the thread that may be waiting in run()
  std::lock_guard guard{_condition.mutex};
  _idle = false;
  _lowLineNumber = lowLine;
  _highLineNumber = highLine;
  _condition.cv.notify_all();
}

bool SenderThread::hasError() {
  bool retFlag = false;
  {
    // flag reset after read to prevent multiple reporting
    //  of errors in ImportHelper
    std::lock_guard guard{_condition.mutex};
    retFlag = _hasError;
    _hasError = false;
  }

  if (retFlag) {
    beginShutdown();
  }
  return retFlag;
}

bool SenderThread::isReady() const {
  std::lock_guard guard{_condition.mutex};
  return _ready;
}

bool SenderThread::isIdle() const {
  std::lock_guard guard{_condition.mutex};
  return _idle;
}

bool SenderThread::isDone() const {
  std::lock_guard guard{_condition.mutex};
  return _idle || _hasError;
}

void SenderThread::run() {
  while (!isStopping()) {
    {
      std::unique_lock guard{_condition.mutex};
      if (_hasError) {
        break;
      }
      _ready = true;
      if (_idle) {
        _condition.cv.wait(guard);
      }
    }
    if (isStopping()) {
      break;
    }
    try {
      if (_data.length() > 0) {
        TRI_ASSERT(!_idle && !_url.empty());

        {
          QuickHistogramTimer timer(_stats->_histogram,
                                    (_highLineNumber - _lowLineNumber) + 1);
          std::unique_ptr<httpclient::SimpleHttpResult> result(_client->request(
              rest::RequestType::POST, _url, _data.c_str(), _data.length()));

          handleResult(result.get());
        }

        _url.clear();
        _data.reset();
      }

      std::lock_guard guard{_condition.mutex};
      _idle = true;
    } catch (...) {
      std::lock_guard guard{_condition.mutex};
      _hasError = true;
      _idle = true;
    }

    _wakeup();
  }

  std::lock_guard guard{_condition.mutex};
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
    std::unique_lock guardError{_condition.mutex};
    // no body, likely error situation
    _errorMessage = result->getHttpReturnMessage();
    // will trigger the waiting ImportHelper thread to cancel the import
    _hasError = true;
  }

  if (haveBody) {
    VPackSlice body = parsedBody->slice();

    // error details
    if (VPackSlice details = body.get("details"); details.isArray()) {
      for (VPackSlice detail : VPackArrayIterator(details)) {
        if (!detail.isString()) {
          continue;
        }
        if (!_stats->logError(detail.stringView())) {
          break;
        }
      }
    }

    {
      // first update all the statistics
      std::lock_guard guard{_stats->_mutex};
      // look up the "created" flag
      _stats->_numberCreated +=
          arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
              body, "created", 0);

      // look up the "errors" flag
      _stats->_numberErrors +=
          arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
              body, "errors", 0);

      // look up the "updated" flag
      _stats->_numberUpdated +=
          arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
              body, "updated", 0);

      // look up the "ignored" flag
      _stats->_numberIgnored +=
          arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
              body, "ignored", 0);
    }

    // get the "error" flag. This returns a pointer, not a copy
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(body, "error",
                                                            false)) {
      std::unique_lock guardError{_condition.mutex};
      // get the error message
      VPackSlice errorMessage = body.get("errorMessage");
      if (errorMessage.isString()) {
        _errorMessage = errorMessage.copyString();
      }

      // will trigger the waiting ImportHelper thread to cancel the import
      _hasError = true;
      return;
    }
  }  // if

  {
    std::unique_lock guardError{_condition.mutex};
    if (!_hasError && !result->getHttpReturnMessage().empty() &&
        !result->isComplete()) {
      _errorMessage = result->getHttpReturnMessage();
      if (0 != _lowLineNumber || 0 != _highLineNumber) {
        LOG_TOPIC("8add8", WARN, arangodb::Logger::FIXME)
            << "Error left import lines " << _lowLineNumber << " through "
            << _highLineNumber << " in unknown state";
      }  // if
      _hasError = true;
    }  // if
  }
}
