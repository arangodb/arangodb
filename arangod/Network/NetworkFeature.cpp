////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "NetworkFeature.h"

#include "Logger/Logger.h"
#include "Network/ConnectionPool.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {
  
std::atomic<network::ConnectionPool*> NetworkFeature::_poolPtr(nullptr);

NetworkFeature::NetworkFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "Network"),
      _numThreads(1),
      _maxOpenConnections(128),
      _requestTimeoutMilli(60 * 1000),
      _connectionTtlMilli(5 * 60 * 1000),
      _verifyHosts(false) {
  setOptional(true);
  startsAfter("Server");
}
  
void NetworkFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("--network", "Networking ");
  
  options->addOption("--network.threads",
                     "number of network IO threads",
                     new UInt64Parameter(&_numThreads));
  options->addOption("--network.max-open-connections",
                     "max open network connections",
                     new UInt64Parameter(&_maxOpenConnections));
  options->addOption("--network.verify-hosts",
                     "verify hosts when using TLS",
                     new BooleanParameter(&_verifyHosts));
}
  
void NetworkFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  if (_numThreads < 1) {
    _numThreads = 1;
  }
  if (_maxOpenConnections < 8) {
    _maxOpenConnections = 8;
  }
  if (_requestTimeoutMilli < 10000) {
    _requestTimeoutMilli = 10000;
  }
  if (_connectionTtlMilli < 10000) {
    _connectionTtlMilli = 10000;
  }
}
  
void NetworkFeature::prepare() {
  _pool = std::make_unique<network::ConnectionPool>(this);
  _poolPtr.store(_pool.get(), std::memory_order_release);
}
  
void NetworkFeature::unprepare() {
  _poolPtr.store(nullptr, std::memory_order_release);
}
  
  
} // arangodb
