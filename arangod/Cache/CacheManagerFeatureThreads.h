////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_MANAGER_FEATURE_THREADS_H
#define ARANGODB_CACHE_MANAGER_FEATURE_THREADS_H

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Cache/Manager.h"
#include "Cache/Rebalancer.h"

#include <stdint.h>

namespace arangodb {

class CacheRebalancerThread final : public Thread {
 public:
  CacheRebalancerThread(cache::Manager* manager, uint64_t interval);
  ~CacheRebalancerThread();

  void beginShutdown() override;

 protected:
  void run() override;

 private:
  cache::Manager* _manager;
  cache::Rebalancer _rebalancer;
  uint64_t _fullInterval;
  uint64_t _shortInterval;
  basics::ConditionVariable _condition;
};

};  // end namespace arangodb

#endif
