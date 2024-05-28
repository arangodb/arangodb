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
};
}  // namespace httpclient
}  // namespace arangodb
