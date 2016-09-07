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

#ifndef ARANGOD_CONSENSUS_STORE_H
#define ARANGOD_CONSENSUS_STORE_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Node.h"

namespace arangodb {
namespace consensus {

class Agent;

/// @brief Key value tree
class Store : public arangodb::Thread {
 public:
  /// @brief Construct with name
  explicit Store(Agent* agent, std::string const& name = "root");

  /// @brief Destruct
  virtual ~Store();

  /// @brief Copy constructor
  Store(Store const& other);

  /// @brief Move constructor
  Store(Store&& other);

  // @brief Copy assignent
  Store& operator=(Store const& rhs);

  // @brief Move assigment
  Store& operator=(Store&& rhs);

  /// @brief Apply entry in query
  std::vector<bool> apply(query_t const& query, bool verbose = false);

  /// @brief Apply entry in query
  std::vector<bool> apply(std::vector<Slice> const& query, bool inform = true);

  /// @brief Read specified query from store
  std::vector<bool> read(query_t const& query, query_t& result) const;

  /// @brief Begin shutdown of thread
  void beginShutdown() override final;

  /// @brief Start thread
  bool start();

  /// @brief Set name
  void name(std::string const& name);

  /// @brief Get name
  std::string const& name() const;

  /// @brief Dump everything to builder
  void dumpToBuilder(Builder&) const;

  /// @brief Notify observers
  void notifyObservers() const;

  /// @brief See how far the path matches anything in store
  size_t matchPath(std::vector<std::string> const& pv) const;

  /// @brief Get node specified by path vector
  Node operator()(std::vector<std::string> const& pv);
  /// @brief Get node specified by path vector
  Node const operator()(std::vector<std::string> const& pv) const;

  /// @brief Get node specified by path string
  Node operator()(std::string const& path);
  /// @brief Get node specified by path string
  Node const operator()(std::string const& path) const;

  Store& operator=(VPackSlice const& slice);

  /// @brief Apply single slice
  bool applies(arangodb::velocypack::Slice const&);

  /// @brief Create Builder representing this store
  void toBuilder(Builder&, bool showHidden = false) const;

  /// @brief Copy out a node
  Node const get(std::string const& path) const;

  std::string toJson() const;

  friend class Node;

  std::vector<std::string> exists(std::string const&) const;

 private:
  /// @brief Remove time to live entries for uri
  void removeTTL(std::string const&);

  std::multimap<TimePoint, std::string>& timeTable();
  std::multimap<TimePoint, std::string> const& timeTable() const;
  std::multimap<std::string, std::string>& observerTable();
  std::multimap<std::string, std::string> const& observerTable() const;
  std::multimap<std::string, std::string>& observedTable();
  std::multimap<std::string, std::string> const& observedTable() const;

  /// @brief Read individual entry specified in slice into builder
  bool read(arangodb::velocypack::Slice const&,
            arangodb::velocypack::Builder&) const;

  /// @brief Check precondition
  bool check(arangodb::velocypack::Slice const&) const;

  /// @brief Clear entries, whose time to live has expired
  query_t clearExpired() const;

  /// @brief Run thread
  void run() override final;

  /// @brief Condition variable guarding removal of expired entries
  mutable arangodb::basics::ConditionVariable _cv;

  /// @brief Read/Write mutex on database
  mutable arangodb::Mutex _storeLock;

  /// @brief My own agent
  Agent* _agent;

  /// @brief Table of expiries in tree (only used in root node)
  std::multimap<TimePoint, std::string> _timeTable;

  /// @brief Table of observers in tree (only used in root node)
  std::multimap<std::string, std::string> _observerTable;
  std::multimap<std::string, std::string> _observedTable;

  /// @brief Root node
  Node _node;
};
}
}

#endif
