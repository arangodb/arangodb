////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_CLIENT_CONNECTION_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_CLIENT_CONNECTION_H 1

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
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new client connection
  //////////////////////////////////////////////////////////////////////////////

  ClientConnection(application_features::ApplicationServer& server,
                   Endpoint* endpoint, double, double, size_t);
  ClientConnection(application_features::ApplicationServer& server,
                   std::unique_ptr<Endpoint>& endpoint, double, double, size_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys a client connection
  //////////////////////////////////////////////////////////////////////////////

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

  bool readClientConnection(arangodb::basics::StringBuffer&, bool& connectionClosed) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return whether the connection is readable
  //////////////////////////////////////////////////////////////////////////////

  bool readable() override;
};
}  // namespace httpclient
}  // namespace arangodb

#endif
