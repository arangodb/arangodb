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

#ifndef __ARANGODB_CONSENSUS_AGENCY_COMMON__
#define __ARANGODB_CONSENSUS_AGENCY_COMMON__

#include <Logger/Logger.h>
#include <Basics/VelocyPackHelper.h>

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <initializer_list>
#include <list>
#include <string>
#include <sstream>
#include <vector>

#include <memory>

namespace arangodb {
namespace consensus {

typedef enum AGENCY_STATUS {
  OK = 0,
  RETRACTED_CANDIDACY_FOR_HIGHER_TERM, // Vote for higher term candidate
                                       // while running. Strange!
  RESIGNED_LEADERSHIP_FOR_HIGHER_TERM, // Vote for higher term candidate
                                       // while leading. Very bad!
  LOWER_TERM_APPEND_ENTRIES_RPC,
  NO_MATCHING_PREVLOG
} status_t;

typedef uint64_t term_t;                           // Term type
typedef uint32_t id_t;                             // Id type

enum role_t {                                      // Role
  FOLLOWER, CANDIDATE, LEADER
};

enum AGENT_FAILURE {
  PERSISTENCE,
  TIMEOUT,
  UNAVAILABLE,
  PRECONDITION
};

/**
 * @brief Agent configuration
 */
template<class T>
inline std::ostream& operator<< (std::ostream& l, std::vector<T> const& v) {
  for (auto const& i : v)
    l << i << ",";
  return l;
}
template<typename T>
inline std::ostream& operator<< (std::ostream& os, std::list<T> const& l) {
  for (auto const& i : l)
    os << i << ",";
  return os;
}


struct  constituent_t {                               // Constituent type
  id_t id;
  std::string endpoint;
};

typedef std::vector<constituent_t>    constituency_t; // Constituency type
typedef uint32_t                      state_t;        // State type
typedef std::chrono::duration<long,std::ratio<1,1000>> duration_t;     // Duration type

using query_t = std::shared_ptr<arangodb::velocypack::Builder>;

struct AgentConfiguration {               
  id_t id;
  double min_ping;
  double max_ping;
  std::string end_point;
  std::vector<std::string> end_points;
  std::string end_point_persist;
  bool notify;
  bool sanity_check;
  AgentConfiguration () : id(0), min_ping(.15), max_ping(.3f), notify(false) {};
  AgentConfiguration (uint32_t i, double min_p, double max_p, std::string ep,
                      std::vector<std::string> const& eps, bool n = false, bool s = false) :
    id(i), min_ping(min_p), max_ping(max_p), end_point(ep), end_points(eps),
    notify(n), sanity_check(s) {
    end_point_persist = end_points[id]; 
  }
  inline size_t size() const {return end_points.size();}
  friend std::ostream& operator<<(std::ostream& o, AgentConfiguration const& c) {
    o << "id(" << c.id << ") min_ping(" << c.min_ping
      << ") max_ping(" << c.max_ping << ") endpoints(" << c.end_points << ")";
    return o;
  }
  inline std::string const toString() const {
    std::stringstream s;
    s << *this;
    return s.str();
  }
  query_t const toBuilder () const {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openObject();
    ret->add("endpoints", VPackValue(VPackValueType::Array));
    for (auto const& i : end_points)
      ret->add(VPackValue(i));
    ret->close();
    ret->add("id",VPackValue(id));
    ret->add("min_ping",VPackValue(min_ping));
    ret->add("max_ping",VPackValue(max_ping));
    ret->close();
    return ret;
  }
};
typedef AgentConfiguration config_t;

struct vote_ret_t {
  query_t result;
  explicit vote_ret_t (query_t res) : result(res) {}
};

struct read_ret_t {
  bool accepted;  // Query processed
  id_t redirect;  // Otherwise redirect to
  std::vector<bool> success;
  query_t result; // Result
  read_ret_t (bool a, id_t id, std::vector<bool> suc = std::vector<bool>(),
              query_t res = nullptr) :
    accepted(a), redirect(id), success(suc), result(res) {}
};

typedef uint64_t index_t;

typedef std::initializer_list<index_t> index_list_t;
struct write_ret_t {
  bool accepted;  // Query processed
  id_t redirect;  // Otherwise redirect to
  std::vector<bool>    applied;
  std::vector<index_t> indices; // Indices of log entries (if any) to wait for
  write_ret_t () : accepted(false), redirect(0) {}
  write_ret_t (bool a, id_t id) : accepted(a), redirect(id) {}
  write_ret_t (bool a, id_t id, std::vector<bool> const& app,
               std::vector<index_t> const& idx) :
    accepted(a), redirect(id), applied(app), indices(idx) {}
};

using namespace std::chrono;
using buffer_t = std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>;
/**
 * @brief State entry
 */
struct log_t {
  index_t      index;
  term_t       term; 
  id_t         leaderId;
  buffer_t     entry;
  milliseconds timestamp;
  log_t (index_t idx, term_t t, id_t lid, buffer_t const& e) :
    index(idx), term(t), leaderId(lid), entry(e), timestamp (
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())) {}
  friend std::ostream& operator<< (std::ostream& o, log_t const& l) {
    o << l.index << " " << l.term << " " << l.leaderId << " "
      << l.entry->toString() << " " << l.timestamp.count();
    return o;
  }
};

enum agencyException {
  QUERY_NOT_APPLICABLE
};

struct append_entries_t {
  term_t term;
  bool success;
  append_entries_t (term_t t, bool s) : term(t), success(s) {}
};

struct collect_ret_t {
  index_t prev_log_index;
  term_t prev_log_term;
  std::vector<index_t> indices;
  collect_ret_t () : prev_log_index(0), prev_log_term(0) {}
  collect_ret_t (index_t pli, term_t plt, std::vector<index_t> const& idx) :
    prev_log_index(pli), prev_log_term(plt), indices(idx) {}
  size_t size() const {return indices.size();}
};

struct priv_rpc_ret_t {
  bool success;
  term_t term;
  priv_rpc_ret_t (bool s, term_t t) : success(s), term(t) {}
};

}}

#endif // __ARANGODB_CONSENSUS_AGENT__

