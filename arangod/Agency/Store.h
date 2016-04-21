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

#ifndef ARANGODB_CONSENSUS_STORE_H
#define ARANGODB_CONSENSUS_STORE_H

#include "Node.h"

//#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {


class Agent;

/// @brief Key value tree 
class Store : public Node, public arangodb::Thread {
  
public:

  /// @brief Construct with name
  explicit Store (std::string const& name = "root");

  /// @brief Destruct
  virtual ~Store ();

  /// @brief Apply entry in query 
  std::vector<bool> apply (query_t const& query);

  /// @brief Apply entry in query 
  std::vector<bool> apply (std::vector<Slice> const& query, bool inform = true);

  /// @brief Read specified query from store
  std::vector<bool> read (query_t const& query, query_t& result) const;
  
  /// @brief Begin shutdown of thread
  void beginShutdown () override final;

  /// @brief Start thread
  bool start ();

  /// @brief Start thread with access to agent
  bool start (Agent*);

  /// @brief Set name
  void name (std::string const& name);

  /// @brief Dump everything to builder
  void dumpToBuilder (Builder&) const;

  /// @brief Notify observers
  void notifyObservers () const;

  /// @brief See how far the path matches anything in store
  size_t matchPath (std::vector<std::string> const& pv) const;

private:
  /// @brief Read individual entry specified in slice into builder
  bool read  (arangodb::velocypack::Slice const&,
              arangodb::velocypack::Builder&) const;

  /// @brief Check precondition
  bool check (arangodb::velocypack::Slice const&) const;

  /// @brief Clear entries, whose time to live has expired
  query_t clearExpired () const;

  /// @brief Run thread
  void run () override final;

  /// @brief Condition variable guarding removal of expired entries
  mutable arangodb::basics::ConditionVariable _cv;

  /// @brief Read/Write mutex on database
  mutable arangodb::Mutex _storeLock;

  /// @brief My own agent
  Agent* _agent;

};

}}

#endif
