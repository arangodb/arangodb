////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_AGENT_INTERFACE_H
#define ARANGOD_CONSENSUS_AGENT_INTERFACE_H 1

#include "Agency/AgencyCommon.h"

namespace arangodb {
namespace consensus {
class AgentInterface {
 public:
  /// @brief Possible outcome of write process
  enum raft_commit_t { OK, UNKNOWN, TIMEOUT };

  struct WriteMode {
    bool _discardStartup;
    bool _privileged;
    WriteMode(bool d = false, bool p = false)
        : _discardStartup(d), _privileged(p) {}
    bool privileged() const { return _privileged; }
    bool discardStartup() const { return _discardStartup; }
    bool operator==(WriteMode const& other) const {
      return other._discardStartup == _discardStartup && other._privileged == _privileged;
    }
    bool operator!=(WriteMode const& other) const {
      return other._discardStartup != _discardStartup || other._privileged != _privileged;
    }
  };

  /// @brief Attempt write
  virtual write_ret_t write(query_t const&, WriteMode const& mode = WriteMode()) = 0;

  /// @brief Attempt write
  virtual trans_ret_t transient(query_t const&) = 0;

  /// @brief Attempt write
  virtual trans_ret_t transact(query_t const&) = 0;

  /// @brief Wait for slaves to confirm appended entries
  virtual raft_commit_t waitFor(index_t last_entry, double timeout = 2.0) = 0;

  /// @brief Wait for slaves to confirm appended entries
  virtual bool isCommitted(index_t last_entry) = 0;

  // Suffice warnings
  virtual ~AgentInterface() = default;
};
}  // namespace consensus
}  // namespace arangodb
#endif
