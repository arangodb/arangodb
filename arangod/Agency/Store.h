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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "AgentInterface.h"
#include "Basics/ConditionVariable.h"
#include "RestServer/arangod.h"

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace arangodb {
namespace consensus {

class Node;

struct check_ret_t {
  bool success;
  query_t failed;

  check_ret_t() : success(true), failed(nullptr) {}

  explicit check_ret_t(bool s) : success(s) {}

  inline bool successful() const { return success; }

  inline void successful(bool s) { success = s; }

  inline void open() {
    TRI_ASSERT(failed == nullptr);
    failed = std::make_shared<VPackBuilder>();
    failed->openArray();
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

enum CheckMode { FIRST_FAIL, FULL };

class Agent;

/// @brief Key value tree
class Store {
 public:
  /// @brief Construct with name
  explicit Store(std::string const& name = "root");
  explicit Store(std::nullptr_t) = delete;

  /// @brief Destruct
  ~Store();

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
  std::vector<apply_ret_t> applyTransactions(
      arangodb::velocypack::Slice query,
      AgentInterface::WriteMode const& wmode = AgentInterface::WriteMode());

  index_t applyTransactions(std::vector<log_t> const& queries);

  /// @brief Apply single transaction in query, here query is an array and the
  /// first entry is a write transaction (i.e. an array of length 1, 2 or 3),
  /// if present, the second entry is a precondition, and the third
  /// entry, if present, is a uuid:
  check_ret_t applyTransaction(velocypack::Slice query);

  /// @brief Apply log entries in query, also process callbacks
  std::vector<bool> applyLogEntries(arangodb::velocypack::Builder const& query,
                                    index_t index, term_t term);

  /// @brief Read multiple entries from store
  std::vector<bool> readMultiple(arangodb::velocypack::Slice query,
                                 arangodb::velocypack::Builder& result) const;

  /// @brief Read individual entry specified in slice into builder
  bool read(arangodb::velocypack::Slice query,
            arangodb::velocypack::Builder& result) const;

  /// @brief Dump everything to builder
  void dumpToBuilder(velocypack::Builder&) const;

  void setNodeValue(VPackSlice s);

  /// @brief Create Builder representing this store
  void toBuilder(velocypack::Builder&, bool showHidden = false) const;

  /// @brief get node from this store.
  /// Unprotected! Caller must guard the store.
  std::shared_ptr<Node const> nodePtr(
      std::string const& path = std::string("/")) const;

  /// @brief Get node at path under mutex and store it in velocypack
  void get(std::string const& path, arangodb::velocypack::Builder& b,
           bool showHidden) const;

  /// @brief Copy out a node
  std::shared_ptr<Node const> get(
      std::string const& path = std::string("/")) const;

  /// @brief Copy out a node
  bool has(std::string const& path = std::string("/")) const;

  std::string toJson() const;

  void clear();

  static std::string normalize(char const* key, size_t length);

  /// @brief Normalize node URIs
  static std::string normalize(std::string const& key) {
    return normalize(key.data(), key.size());
  }

  /// @brief Split strings by forward slashes, omitting empty strings,
  /// and ignoring multiple subsequent forward slashes
  static std::vector<std::string> split(std::string_view str);

  using AgencyTriggerCallback =
      std::function<void(std::string_view path, VPackSlice trx)>;

  void registerPrefixTrigger(std::string const& prefix, AgencyTriggerCallback);

 private:
  /// @brief Apply single slice
  bool applies(arangodb::velocypack::Slice const&);

  friend class consensus::Node;
  /// @brief Check precondition
  check_ret_t check(arangodb::velocypack::Slice slice,
                    CheckMode = FIRST_FAIL) const;

 private:
  /// @brief Read/Write mutex on database
  /// guard _node, _timeTable, _observerTable, _observedTable
  mutable std::mutex _storeLock;

  /// @brief Root node
  std::shared_ptr<Node const> _node;

  struct AgencyTrigger {
    explicit AgencyTrigger(AgencyTriggerCallback callback)
        : callback(std::move(callback)) {}
    AgencyTriggerCallback callback;
  };

  void callTriggers(std::string_view key, std::string_view op, VPackSlice trx);
  std::mutex _triggersMutex;
  std::map<std::string, AgencyTrigger, std::less<>> _triggers;
};

inline std::ostream& operator<<(std::ostream& o, Store const& store);

}  // namespace consensus
}  // namespace arangodb
