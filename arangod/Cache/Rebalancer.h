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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_REBALANCER_H
#define ARANGODB_CACHE_REBALANCER_H

#include "Manager.h"

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Dedicated class to rebalance Manager.
////////////////////////////////////////////////////////////////////////////////
class Rebalancer {
 public:
  // Initialize state with no open transactions.
  explicit Rebalancer(Manager* manager);

  // Rebalance the manager.
  int rebalance();

 private:
  Manager* _manager;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
