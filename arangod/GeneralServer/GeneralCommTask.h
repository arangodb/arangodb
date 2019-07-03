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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H 1

#include "GeneralServer/CommTask.h"
#include "GeneralServer/AsioSocket.h"

namespace arangodb {
namespace rest {

template<SocketType T>
class GeneralCommTask : public CommTask {
  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  GeneralCommTask(GeneralServer& server,
                  char const* name,
                  ConnectionInfo,
                  std::unique_ptr<AsioSocket<T>>);

  virtual ~GeneralCommTask();

  void start() override final;
  void close() override final;
  
 protected:
  
  /// read from socket
  void asyncReadSome();
  /// called to process data in _readBuffer, return false to stop
  virtual bool readCallback(asio_ns::error_code ec) = 0;
  
  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  
 protected:
  std::unique_ptr<AsioSocket<T>> _protocol;
};
}  // namespace rest
}  // namespace arangodb

#endif
