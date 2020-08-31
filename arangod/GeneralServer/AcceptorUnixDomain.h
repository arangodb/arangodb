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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGOD_GENERAL_SERVER_ACCEPTORUNIXDOMAIN_H
#define ARANGOD_GENERAL_SERVER_ACCEPTORUNIXDOMAIN_H 1

#include "GeneralServer/Acceptor.h"
#include "GeneralServer/AsioSocket.h"

namespace arangodb {
namespace rest {

class AcceptorUnixDomain final : public Acceptor {
 public:
  AcceptorUnixDomain(rest::GeneralServer& server, rest::IoContext& ctx, Endpoint* endpoint)
      : Acceptor(server, ctx, endpoint), _acceptor(ctx.io_context) {}

 public:
  void open() override;
  void close() override;
  void cancel() override;
  void asyncAccept() override;

 private:
  asio_ns::local::stream_protocol::acceptor _acceptor;
  std::unique_ptr<AsioSocket<SocketType::Unix>> _asioSocket;
};
}  // namespace rest
}  // namespace arangodb

#endif
