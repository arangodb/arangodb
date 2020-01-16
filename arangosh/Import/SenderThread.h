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

#ifndef ARANGODB_IMPORT_SEND_THREAD_H
#define ARANGODB_IMPORT_SEND_THREAD_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/StringBuffer.h"
#include "Basics/Thread.h"
#include "SimpleHttpClient/SimpleHttpClient.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace basics {
class StringBuffer;
}
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

namespace import {
struct ImportStatistics;

class SenderThread final : public arangodb::Thread {
 private:
  SenderThread(SenderThread const&) = delete;
  SenderThread& operator=(SenderThread const&) = delete;

 public:
  explicit SenderThread(application_features::ApplicationServer& server,
                        std::unique_ptr<httpclient::SimpleHttpClient>,
                        ImportStatistics* stats, std::function<void()> const& wakeup);

  ~SenderThread();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief imports a delimited file
  //////////////////////////////////////////////////////////////////////////////

  void sendData(std::string const& url, basics::StringBuffer* sender,
                size_t lowLine = 0, size_t highLine = 0);

  bool hasError();
  /// Ready to start sending
  bool isReady();
  /// Currently not sending data
  bool isIdle();
  bool isDone();

  std::string const& errorMessage() const { return _errorMessage; }

  void beginShutdown() override;

 protected:
  void run() override;

 private:
  basics::ConditionVariable _condition;
  std::unique_ptr<httpclient::SimpleHttpClient> _client;
  std::function<void()> _wakeup;
  std::string _url;
  basics::StringBuffer _data;
  bool _hasError;
  bool _idle;
  bool _ready;
  size_t _lowLineNumber;
  size_t _highLineNumber;

  ImportStatistics* _stats;
  std::string _errorMessage;
  void handleResult(httpclient::SimpleHttpResult* result);
};
}  // namespace import
}  // namespace arangodb
#endif
