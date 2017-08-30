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

struct check_ret_t {

  bool success;
  query_t failed;
  
  check_ret_t() : success(true), failed(nullptr) {}
  
  explicit check_ret_t(bool s) : success(s) {}
  
  inline bool successful() const { return success; }
  
  inline void successful(bool s) { success = s; }

  inline void open() {
    TRI_ASSERT(failed == nullptr);
    failed = std::make_shared<VPackBuilder>(); failed->openArray();
  }
  
  inline void push_back(VPackSlice const& key) {
    TRI_ASSERT(failed != nullptr);
    success = false;
    failed->add(key);
  }
  
  inline void close() {
    TRI_ASSERT(failed != nullptr);
    failed->close();
  } 
  
};

enum CheckMode {FIRST_FAIL, FULL};

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

  /// @brief Apply entry in query, query must be an array of individual
  /// transactions that are in turn arrays with 1, 2 or 3 entries as described
  /// in the next method.
  std::vector<bool> applyTransactions(query_t const& query);

  /// @brief Apply single transaction in query, here query is an array and the
  /// first entry is a write transaction (i.e. an array of length 1, 2 or 3), 
  /// if present, the second entry is a precondition, and the third
  /// entry, if present, is a uuid:
  check_ret_t applyTransaction(Slice const& query);

  /// @brief Apply log entries in query, also process callbacks
  std::vector<bool> applyLogEntries(arangodb::velocypack::Builder const& query,
                          index_t index, term_t term, bool inform);

  /// @brief Read specified query from store
  std::vector<bool> read(query_t const& query, query_t& result) const;

  /// @brief Read individual entry specified in slice into builder
  bool read(arangodb::velocypack::Slice const&,
            arangodb::velocypack::Builder&) const;
  
  /// @brief Begin shutdown of thread
  void beginShutdown() override final;

  /// @brief Start thread
  bool start();

  /// @brief Dump everything to builder
  void dumpToBuilder(Builder&) const;

  /// @brief Notify observers
  void notifyObservers() const;

  /// @brief See how far the path matches anything in store
  size_t matchPath(std::vector<std::string> const& pv) const;

  Store& operator=(VPackSlice const& slice);

  /// @brief Create Builder representing this store
  void toBuilder(Builder&, bool showHidden = false) const;

  /// @brief Copy out a node
  Node get(std::string const& path = std::string("/")) const;

  /// @brief Copy out a node
  bool has(std::string const& path = std::string("/")) const;

  std::string toJson() const;

  void clear();

  friend class Node;

  /// @brief Apply single slice
  bool applies(arangodb::velocypack::Slice const&);
 
 private:

  /// @brief Remove time to live entries for uri
  void removeTTL(std::string const&);

  std::multimap<TimePoint, std::string>& timeTable();
  std::multimap<TimePoint, std::string> const& timeTable() const;
  std::unordered_multimap<std::string, std::string>& observerTable();
  std::unordered_multimap<std::string, std::string> const& observerTable() const;
  std::unordered_multimap<std::string, std::string>& observedTable();
  std::unordered_multimap<std::string, std::string> const& observedTable() const;

  /// @brief Check precondition
  check_ret_t check(arangodb::velocypack::Slice const&, CheckMode = FIRST_FAIL) const;

  /// @brief Clear entries, whose time to live has expired
  query_t clearExpired() const;

  /// @brief Run thread
  void run() override final;

 private:
  /// @brief Condition variable guarding removal of expired entries
  mutable arangodb::basics::ConditionVariable _cv;

  /// @brief Read/Write mutex on database
  /// guard _node, _timeTable, _observerTable, _observedTable
  mutable arangodb::Mutex _storeLock;
 
  /// @brief My own agent
  Agent* _agent;

  /// @brief Table of expiries in tree (only used in root node)
  std::multimap<TimePoint, std::string> _timeTable;

  /// @brief Table of observers in tree (only used in root node)
  std::unordered_multimap<std::string, std::string> _observerTable;
  std::unordered_multimap<std::string, std::string> _observedTable;

  /// @brief Root node
  Node _node;
};


inline std::ostream& operator<<(std::ostream& o, Store const& store) {
  return store.get().print(o);
}

}
}

#endif
