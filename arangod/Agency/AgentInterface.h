////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Agency/AgencyCommon.h"
#include "Agency/AgentConfiguration.h"

namespace arangodb::velocypack {
class Slice;
}

namespace arangodb::consensus {

class AgentInterface {
 public:
  // Suffice warnings
  virtual ~AgentInterface() = default;

  /// @brief Possible outcome of write process
  enum raft_commit_t { OK, UNKNOWN, TIMEOUT };

  struct WriteMode {
    bool _discardStartup;
    bool _privileged;
    WriteMode(bool d = false, bool p = false) noexcept
        : _discardStartup(d), _privileged(p) {}
    bool privileged() const noexcept { return _privileged; }
    bool discardStartup() const noexcept { return _discardStartup; }
    bool operator==(WriteMode const& other) const noexcept {
      return other._discardStartup == _discardStartup &&
             other._privileged == _privileged;
    }
    bool operator!=(WriteMode const& other) const noexcept {
      return other._discardStartup != _discardStartup ||
             other._privileged != _privileged;
    }
  };

  /// @brief Attempt write
  virtual write_ret_t write(velocypack::Slice query,
                            WriteMode const& mode = WriteMode()) = 0;

  /// @brief Attempt write
  virtual trans_ret_t transient(velocypack::Slice query) = 0;

  /// @brief Attempt write
  virtual trans_ret_t transact(velocypack::Slice qs) = 0;

  /// @brief Wait for followers to confirm appended entries
  virtual raft_commit_t waitFor(index_t last_entry, double timeout = 2.0) = 0;

  /// @brief Wait for followers to confirm appended entries
  virtual bool isCommitted(index_t last_entry) const = 0;

  /// @brief Provide configuration
  virtual config_t const& config() const = 0;
};

}  // namespace arangodb::consensus
