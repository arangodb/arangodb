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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stddef.h>
#include <memory>

#include "SimpleHttpClient/GeneralClientConnection.h"

namespace arangodb {
class Endpoint;
namespace basics {
class StringBuffer;
}
namespace httpclient {

////////////////////////////////////////////////////////////////////////////////
/// @brief client connection
////////////////////////////////////////////////////////////////////////////////

class ClientConnection final : public GeneralClientConnection {
 private:
  ClientConnection(ClientConnection const&);
  ClientConnection& operator=(ClientConnection const&);

 public:
  ClientConnection(application_features::CommunicationFeaturePhase& comm,
                   Endpoint* endpoint, double, double, size_t);
  ClientConnection(application_features::CommunicationFeaturePhase& comm,
                   std::unique_ptr<Endpoint>& endpoint, double, double, size_t);

  ~ClientConnection();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief connect
  //////////////////////////////////////////////////////////////////////////////

  bool connectSocket() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief disconnect
  //////////////////////////////////////////////////////////////////////////////

  void disconnectSocket() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief write data to the connection
  //////////////////////////////////////////////////////////////////////////////

  bool writeClientConnection(void const*, size_t, size_t*) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief read data from the connection
  //////////////////////////////////////////////////////////////////////////////

  bool readClientConnection(arangodb::basics::StringBuffer&,
                            bool& connectionClosed) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return whether the connection is readable
  //////////////////////////////////////////////////////////////////////////////

  bool readable() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether or not an idle TCP/IP connection is still alive
  /// This method is intended to be used for TCP/IP connections only and only
  /// on known idle connections (see below)!
  /// If the kernel is aware of the fact that the connection is broken,
  /// this is not immediately visible to the application with any read or
  /// write operation. Therefore, if a connection has been idle for some time,
  /// it might have been broken without the application noticing it. This
  /// can for example happen if the connection is taken from a connection
  /// cache. This method does a non-invasive non-blocking `recv` call to see
  /// if the connection is still alive. Interpretation of results:
  ///   If the recv call returns 0, the connection is broken. In this case we
  ///   return `false`.
  ///   If the recv call returns -1, the connection is still alive and errno
  ///   is set to EAGAIN == EWOULDBLOCK. In this case we return `true`.
  ///   If something has been received on the socket, the recv call will return
  ///   a positive number. In this case we return `false` as well, since we
  ///   are assuming the the connection is idle and bad things would happen
  ///   if we continue to use it anyway. This includes the following important
  ///   case: If the connection is actually a TLS connection, the other side
  ///   might have sent a "Notify: Close" TLS message to close the connection.
  ///   If the connection was in a connection cache and thus has not read
  ///   data recently, the TLS layer might not have noticed the close message.
  ///   As a consequence the actual TCP/IP connection is not yet closed, but
  ///   it is dead in the water, since the very next time we try to read or
  ///   write data, the TLS layer will notice the close message and close the
  ///   connection right away.
  //////////////////////////////////////////////////////////////////////////////

  bool test_idle_connection() override;
};
}  // namespace httpclient
}  // namespace arangodb
