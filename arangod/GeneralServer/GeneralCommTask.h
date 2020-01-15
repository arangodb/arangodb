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

#include "GeneralServer/AsioSocket.h"
#include "GeneralServer/CommTask.h"

namespace arangodb {
namespace rest {

template <SocketType T>
class GeneralCommTask : public CommTask {
  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  GeneralCommTask(GeneralServer& server, ConnectionInfo,
                  std::unique_ptr<AsioSocket<T>>);

  virtual ~GeneralCommTask() = default;

  void start() override;
  void stop() override;
  
  void close(asio_ns::error_code const& err = asio_ns::error_code());

 protected:
  
  /// set / reset connection timeout
  void setTimeout(std::chrono::milliseconds millis);
  
  /// read from socket
  void asyncReadSome();
  
  protected:
  
  // set a read timeout in asyncReadSome
  virtual bool enableReadTimeout() const = 0;
  
  /// called to process data in _readBuffer, return false to stop
  virtual bool readCallback(asio_ns::error_code ec) = 0;

 protected:
  
  /// default max chunksize is 30kb in arangodb (each read fits)
  static constexpr size_t ReadBlockSize = 1024 * 32;
  
  static constexpr std::chrono::seconds DefaultTimeout{120};
  
  std::unique_ptr<AsioSocket<T>> _protocol;
};
}  // namespace rest
}  // namespace arangodb

#endif
