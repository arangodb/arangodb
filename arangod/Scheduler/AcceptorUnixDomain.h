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

#ifndef ARANGOD_SCHEDULER_ACCEPTORUNIXDOMAIN_H
#define ARANGOD_SCHEDULER_ACCEPTORUNIXDOMAIN_H 1

#include "Scheduler/Acceptor.h"

namespace arangodb {
class AcceptorUnixDomain final : public Acceptor {
 public:
  AcceptorUnixDomain(rest::Scheduler* scheduler, Endpoint* endpoint)
      : Acceptor(scheduler, endpoint),
        _acceptor(scheduler->newDomainAcceptor()) {}

 public:
  void open() override;
  void close() override;
  void asyncAccept(AcceptHandler const& handler) override;

 private:
  std::unique_ptr<asio_ns::local::stream_protocol::acceptor> _acceptor;
};
}

#endif
