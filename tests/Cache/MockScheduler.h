////////////////////////////////////////////////////////////////////////////////
/// @brief helper for cache suite
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef UNITTESTS_CACHE_MOCK_SCHEDULER_H
#define UNITTESTS_CACHE_MOCK_SCHEDULER_H

#include "Basics/Common.h"

#include "Basics/asio-helper.h"

#include <memory>
#include <thread>
#include <vector>

namespace arangodb {
namespace cache {

class MockScheduler {
  typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
  std::unique_ptr<boost::asio::io_service> _ioService;
  std::unique_ptr<boost::asio::io_service::work> _serviceGuard;
  std::vector<std::thread*> _group;

 public:
  MockScheduler(size_t threads);
  ~MockScheduler();
  void post(std::function<void()> fn);
};

};  // end namespace cache
};  // end namespace arangodb

#endif
