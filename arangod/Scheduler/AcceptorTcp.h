////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_SCHEDULER_ACCEPTORTCP_H
#define ARANGOD_SCHEDULER_ACCEPTORTCP_H 1

#include "Scheduler/Acceptor.h"

namespace arangodb {
class AcceptorTcp final : public Acceptor {
 public:
  AcceptorTcp(rest::GeneralServer &server,
              rest::GeneralServer::IoContext &context, Endpoint* endpoint)
      : Acceptor(server, context, endpoint), _acceptor(context.newAcceptor()) {}

 public:
  void open() override;
  void close() override {
    _acceptor->close();
    if (_peer) {
      asio_ns::error_code ec;
      _peer->close(ec);
    }
  };
  void asyncAccept(Acceptor::AcceptHandler const& handler) override;

 private:
  std::unique_ptr<asio_ns::ip::tcp::acceptor> _acceptor;
};
}

#endif
