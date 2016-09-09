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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_AGENCY_COMMON_H
#define ARANGOD_CONSENSUS_AGENCY_COMMON_H 1

#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <memory>

namespace arangodb {
namespace consensus {

/// @brief Term type
typedef uint64_t term_t;

/// @brief Agent roles
enum role_t { FOLLOWER, CANDIDATE, LEADER };

static const std::string NO_LEADER = "";

/// @brief Duration type
typedef std::chrono::duration<long, std::ratio<1, 1000>> duration_t;

/// @brief Log query type
using query_t = std::shared_ptr<arangodb::velocypack::Builder>;

/// @brief Log entry index type
typedef uint64_t index_t;

/// @brief Read request return type
struct read_ret_t {
  bool accepted;              ///< @brief Query accepted (i.e. we are leader)
  std::string redirect;       ///< @brief If not accepted redirect id
  std::vector<bool> success;  ///< @brief Query's precond OK
  query_t result;             ///< @brief Query result
  read_ret_t(bool a, std::string const& id,
             std::vector<bool> const& suc = std::vector<bool>(), query_t res = nullptr)
      : accepted(a), redirect(id), success(suc), result(res) {}
};

/// @brief Write request return type
struct write_ret_t {
  bool accepted;         ///< @brief Query accepted (i.e. we are leader)
  std::string redirect;  ///< @brief If not accepted redirect id
  std::vector<bool> applied;
  std::vector<index_t> indices;  // Indices of log entries (if any) to wait for
  write_ret_t() : accepted(false), redirect("") {}
  write_ret_t(bool a, std::string const& id) : accepted(a), redirect(id) {}
  write_ret_t(bool a, std::string const& id, std::vector<bool> const& app,
              std::vector<index_t> const& idx)
      : accepted(a), redirect(id), applied(app), indices(idx) {}
};

/// @brief Buffer type
using buffer_t = std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>;

/// @brief State entry
struct log_t {
  index_t index;                        ///< @brief Log index
  term_t term;                          ///< @brief Log term
  buffer_t entry;                       ///< @brief To log
  std::chrono::milliseconds timestamp;  ///< @brief Timestamp

  log_t(index_t idx, term_t t, buffer_t const& e)
      : index(idx),
        term(t),
        entry(e),
        timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())) {}

  friend std::ostream& operator<<(std::ostream& o, log_t const& l) {
    o << l.index << " " << l.term << " " << VPackSlice(l.entry->data()).toJson()
      << " " << l.timestamp.count();
    return o;
  }
};


static std::string const pubApiPrefix = "/_api/agency/";
static std::string const privApiPrefix = "/_api/agency_priv/";

/// @brief Private RPC return type
struct priv_rpc_ret_t {
  bool success;
  term_t term;
  priv_rpc_ret_t(bool s, term_t t) : success(s), term(t) {}
};
}
}

#endif
