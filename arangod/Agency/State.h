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

#ifndef __ARANGODB_CONSENSUS_STATE__
#define __ARANGODB_CONSENSUS_STATE__


#include "AgencyCommon.h"
#include "State.h"

#include <Basics/Thread.h>
#include <Cluster/ClusterComm.h>
#include <velocypack/vpack.h>

#include <cstdint>
#include <deque>
#include <functional>

struct TRI_vocbase_t;

namespace arangodb {
class ApplicationV8;
namespace aql {
class QueryRegistry;
}

namespace consensus {

class Agent;

/**
 * @brief State replica
 */
class State {
  
public:
  
  /// @brief Default constructor
  explicit State (std::string const& end_point = "tcp://localhost:8529");
  

  /// @brief Default Destructor
  virtual ~State();
  

  /// @brief Append log entry
  void append (query_t const& query);


  /// @brief Log entries (leader)
  std::vector<index_t> log (query_t const& query, std::vector<bool> const& indices, term_t term, id_t lid);


  /// @brief Log entries (followers)
  bool log (query_t const& queries, term_t term, id_t leaderId, index_t prevLogIndex, term_t prevLogTerm);


  /// @brief Find entry at index with term
  bool find (index_t index, term_t term);


  /// @brief Get complete log entries bound by lower and upper bounds.
  ///        Default: [first, last]
  std::vector<log_t> get (
    index_t = 0, index_t = (std::numeric_limits<uint64_t>::max)()) const;

  /// @brief Get complete logged commands by lower and upper bounds.
  ///        Default: [first, last]
  std::vector<VPackSlice> slices (
    index_t = 0, index_t = (std::numeric_limits<uint64_t>::max)()) const;


  /// @brief log entry at index i
  log_t const& operator[](index_t) const;


  /// @brief last log entry
  log_t const& lastLog () const;


  /// @brief Set endpoint
  bool setEndPoint (std::string const&);


  /// @brief Load persisted data from above or start with empty log
  bool loadCollections (TRI_vocbase_t*, ApplicationV8*,
                        aql::QueryRegistry*, bool);

  /// @brief Pipe to ostream
  friend std::ostream& operator<< (std::ostream& os, State const& s) {
    for (auto const& i : s._log)
      LOG_TOPIC(INFO, Logger::AGENCY)
        << "index(" << i.index <<") term(" << i.term << ") leader: ("
        << i.leaderId << ") query(" << VPackSlice(i.entry->data()).toJson()
        << ")";
    return os;
  }

private:

  bool snapshot ();

  /// @brief Save currentTerm, votedFor, log entries
  bool persist (index_t index, term_t term, id_t lid,
                arangodb::velocypack::Slice const& entry);

  /// @brief Load collection from persistent store
  bool loadCollection (std::string const& name);

  /// @brief Check collections
  bool checkCollections();

  /// @brief Check collection sanity
  bool checkCollection(std::string const& name);

  /// @brief Create collections
  bool createCollections();

  /// @brief Create collection
  bool createCollection(std::string const& name);

  bool compact ();

  TRI_vocbase_t* _vocbase;
  ApplicationV8* _applicationV8;
  aql::QueryRegistry* _queryRegistry;

  mutable arangodb::Mutex _logLock;  /**< @brief Mutex for modifying _log */
  std::deque<log_t> _log;           /**< @brief  State entries */
  std::string _end_point;            /**< @brief persistence end point */
  bool _collections_checked;                 /**< @brief Collections checked */
  bool _collections_loaded;

  OperationOptions _options;

  
};

}}

#endif
