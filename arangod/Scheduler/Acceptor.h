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

#ifndef ARANGOD_SCHEDULER_ACCEPTOR_H
#define ARANGOD_SCHEDULER_ACCEPTOR_H 1

#include "Endpoint/Endpoint.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Scheduler/Socket.h"
#include "Ssl/SslServerFeature.h"

namespace arangodb {
class Acceptor {
 public:
  typedef std::function<void(boost::system::error_code const&)> AcceptHandler;

  Acceptor(boost::asio::io_service& ioService, Endpoint* endpoint);
  virtual ~Acceptor() {}

  virtual void open() = 0;
  virtual void close() = 0;
  virtual void asyncAccept(AcceptHandler const& handler) = 0;
  std::unique_ptr<Socket> movePeer() { return std::move(_peer); };

 public:
  static std::unique_ptr<Acceptor> factory(boost::asio::io_service& _ioService,
                                           Endpoint* endpoint);

 protected:
  virtual void createPeer() = 0;

 protected:
  boost::asio::io_service& _ioService;
  Endpoint* _endpoint;
  std::unique_ptr<Socket> _peer;
};
}  // namespace arangodb
#endif
