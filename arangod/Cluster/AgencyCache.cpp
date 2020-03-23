////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB Inc, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCache.cpp"

// Get Builder from readDB mainly /Plan /Current
VPackBuider const get(std::string const& path) const { 
  std::shared_lock(_lock);
  _readDB.toBuilder(path);
}

void run() {

  // _commitIndex = 0, _readDB = {}, _agentEP = random(agents)
  // while (not stopping) {
  //   res = GET _api/agency/poll?index=_commitIndex timeout(60s)
  //   if (res.code == 200) {
  //     if (res.body) {
  //       if (complete readDB) {
  //         _readDB = res.body.readDB
  //         _commitIndex = res.body.commitIndex
  //       } else { 
  //         _read.DB.apply(res.body.logs)
  //       }
  //     }
  //   } else if (503) {
  //     wait(1s)
  //     _agent = random(agents)
  //   } else if (307) {
  //     _agent = body.agent
  //   }
  // }
  
}
