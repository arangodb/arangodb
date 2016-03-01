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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VELOCY_SERVER_VELOCYS_COMM_TASK_H
#define ARANGOD_VELOCY_SERVER_VELOCYS_COMM_TASK_H 1

#include "VelocyServer/VelocyCommTask.h"

#include <openssl/ssl.h>


namespace arangodb {
namespace rest {
class VelocysServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief velocys communication
////////////////////////////////////////////////////////////////////////////////

class VelocysCommTask : public ArangosCommTask, public VelocyCommTask {
  VelocysCommTask(VelocysCommTask const&) = delete;
  VelocysCommTask const& operator=(VelocysCommTask const&) = delete;

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief read block size
  //////////////////////////////////////////////////////////////////////////////

  static size_t const READ_BLOCK_SIZE = 10000;

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new task with a given socket
  //////////////////////////////////////////////////////////////////////////////

  VelocysCommTask(VelocysServer*, TRI_socket_t, ConnectionInfo const&,
                double keepAliveTimeout, SSL_CTX* ctx, int verificationMode,
                             GeneralRequest::ProtocolVersion version, GeneralRequest::RequestType request,
                int (*verificationCallback)(int, X509_STORE_CTX*));

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destructs a task
  //////////////////////////////////////////////////////////////////////////////

 protected:
  ~VelocysCommTask();

  
 protected:

  bool setup(Scheduler*, EventLoop) override;


  bool handleEvent(EventToken, EventType) override;

};
}
}

#endif