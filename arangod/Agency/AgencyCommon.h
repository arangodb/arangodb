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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_AGENCY_COMMON_H
#define ARANGOD_CONSENSUS_AGENCY_COMMON_H 1

#include <chrono>

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace consensus {

// The following are instanciated in Agent.cpp:
extern std::string const pubApiPrefix;
extern std::string const privApiPrefix;
extern std::string const NO_LEADER;

enum role_t { FOLLOWER, CANDIDATE, LEADER };

enum apply_ret_t { APPLIED, PRECONDITION_FAILED, FORBIDDEN, UNKNOWN_ERROR };

typedef std::chrono::duration<long, std::ratio<1, 1000>> duration_t;
typedef uint64_t index_t;
typedef uint64_t term_t;

using buffer_t = std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>;
using query_t = std::shared_ptr<arangodb::velocypack::Builder>;

struct read_ret_t {
  bool accepted;              // Query accepted (i.e. we are leader)
  std::string redirect;       // If not accepted redirect id
  std::vector<bool> success;  // Query's precond OK
  query_t result;             // Query result

  read_ret_t(bool a, std::string const& id) : accepted(a), redirect(id) {}

  read_ret_t(bool a, std::string const& id,
             std::vector<bool>&& suc, query_t&& res)
      : accepted(a), redirect(id), success(std::move(suc)), result(std::move(res)) {}
};

struct write_ret_t {
  bool accepted;         // Query accepted (i.e. we are leader)
  std::string redirect;  // If not accepted redirect id
  std::vector<apply_ret_t> applied;
  std::vector<index_t> indices;  // Indices of log entries (if any) to wait for
  write_ret_t() : accepted(false), redirect("") {}
  write_ret_t(bool a, std::string const& id) : accepted(a), redirect(id) {}
  write_ret_t(bool a, std::string const& id, std::vector<apply_ret_t> const& app,
              std::vector<index_t> const& idx)
      : accepted(a), redirect(id), applied(app), indices(idx) {}
  bool successful() const {
    return !indices.empty() &&
           std::find(indices.begin(), indices.end(), 0) == indices.end();
  }
};

inline std::ostream& operator<<(std::ostream& o, write_ret_t const& w) {
  o << "accepted: " << w.accepted << ", redirect: " << w.redirect << ", indices: [";
  for (const auto& i : w.indices) {
    o << i << ", ";
  }
  o << "]";
  return o;
}

struct trans_ret_t {
  bool accepted;         // Query accepted (i.e. we are leader)
  std::string redirect;  // If not accepted redirect id
  index_t maxind;
  size_t failed;
  query_t result;
  trans_ret_t() : accepted(false), redirect(""), maxind(0), failed(0) {}
  trans_ret_t(bool a, std::string const& id)
      : accepted(a), redirect(id), maxind(0), failed(0) {}
  trans_ret_t(bool a, std::string const& id, index_t mi, size_t f, query_t const& res)
      : accepted(a), redirect(id), maxind(mi), failed(f), result(res) {}
};

struct log_t {
  index_t index;                        // Log index
  term_t term;                          // Log term
  buffer_t entry;                       // To log
  std::string clientId;                 // Client ID
  std::chrono::milliseconds timestamp;  // Timestamp

  /// @brief creates a log entry, fully copying it from the buffer
  log_t(index_t idx, term_t t, buffer_t const& e,
        std::string const& clientId,
        uint64_t const& m = 0)
      : index(idx),
        term(t),
        entry(std::make_shared<arangodb::velocypack::Buffer<uint8_t>>(*e.get())),
        clientId(clientId),
        timestamp(m) {}
  
  /// @brief creates a log entry, taking over the buffer
  log_t(index_t idx, term_t t, buffer_t&& e,
        std::string const& clientId,
        uint64_t const& m = 0)
      : index(idx),
        term(t),
        entry(std::move(e)),
        clientId(clientId),
        timestamp(m) {}

  void toVelocyPack(velocypack::Builder& builder) const {
    builder.openObject();

    builder.add("index", VPackValue(index));
    builder.add("term", VPackValue(term));
    builder.add("query", VPackSlice(entry->data()));
    builder.add("clientId", VPackValue(clientId));
    builder.add("timestamp", VPackValue(timestamp.count()));
    
    builder.close();
  }

  friend std::ostream& operator<<(std::ostream& o, log_t const& l) {
    o << l.index << " " << l.term << " " << VPackSlice(l.entry->data()).toJson() << " "
      << " " << l.clientId << " " << l.timestamp.count();
    return o;
  }
};

struct priv_rpc_ret_t {
  bool success;
  term_t term;
  priv_rpc_ret_t(bool s, term_t t) : success(s), term(t) {}
};

}  // namespace consensus
}  // namespace arangodb

inline std::ostream& operator<<(std::ostream& o, arangodb::consensus::log_t const& l) {
  o << l.index << " " << l.term << " " << VPackSlice(l.entry->data()).toJson() << " "
    << " " << l.clientId << " " << l.timestamp.count();
  return o;
}

#endif
