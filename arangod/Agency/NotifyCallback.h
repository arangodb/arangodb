////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_NOTIFY_CALLBACK_H
#define ARANGOD_CONSENSUS_NOTIFY_CALLBACK_H 1

#include "Agency/AgencyCommon.h"
#include "Cluster/ClusterComm.h"

#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

class NotifyCallback : public arangodb::ClusterCommCallback {
 public:
  explicit NotifyCallback(std::function<void(bool)> const&);

  virtual bool operator()(arangodb::ClusterCommResult*) override final;

  void shutdown();

 private:
  std::function<void(bool)> _cb;
};
}
}  // namespace

#endif
