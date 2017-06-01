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
namespace basics {
class StringBuffer;
}
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}

namespace import {
struct ImportStatistics;

class SenderThread : public arangodb::Thread {
 private:
  SenderThread(SenderThread const&) = delete;
  SenderThread& operator=(SenderThread const&) = delete;

 public:
  explicit SenderThread(std::unique_ptr<httpclient::SimpleHttpClient>&&,
                        ImportStatistics* stats);

  ~SenderThread();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief imports a delimited file
  //////////////////////////////////////////////////////////////////////////////

  void sendData(std::string const& url, basics::StringBuffer* sender);

  bool idle() const { return _idle; }

  bool hasError() const { return _hasError; }

  std::string const& errorMessage() const { return _errorMessage; }

  void beginShutdown() override;

 protected:
  void run() override;

 private:
  basics::ConditionVariable _condition;
  httpclient::SimpleHttpClient* _client;
  std::string _url;
  basics::StringBuffer _data;
  bool _hasError = false;
  bool _idle = true;

  ImportStatistics* _stats;
  std::string _errorMessage;
  void handleResult(httpclient::SimpleHttpResult* result);
};
}
}
#endif
