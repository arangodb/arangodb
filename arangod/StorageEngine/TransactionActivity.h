////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Activities/GuardedActivity.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"

#include <rocksdb/c.h>
#include <velocypack/Builder.h>

#include <memory>
#include <vector>

namespace arangodb::transaction::activity {

enum class LockStatus { NOT_HOLDING, ACQUIRING, HOLDING, FAILED };

template<typename Inspector>
auto inspect(Inspector& f, LockStatus l) {
  return f.enumeration(l).values(LockStatus::NOT_HOLDING, "not_holding",
                                 LockStatus::ACQUIRING, "acquiring",
                                 LockStatus::HOLDING, "holding",
                                 LockStatus::FAILED, "failed");
}

struct TransactionCollection {
  std::string name;
  DataSourceId cid;
  AccessMode::Type accessType;
  LockStatus lockStatus;

  template<typename Inspector>
  inline friend auto inspect(Inspector& f, TransactionCollection& d) {
    return f.object(d).fields(f.field("name", d.name),              //
                              f.field("cid", d.cid),                //
                              f.field("accessType", d.accessType),  //
                              f.field("lockStatus", d.lockStatus));
  }
};

struct TransactionActivityData {
  std::string user;
  std::string database;
  TransactionId tid;
  Status status;
  std::vector<TransactionCollection> collections;

  template<typename Inspector>
  inline friend auto inspect(Inspector& f, TransactionActivityData& d) {
    return f.object(d).fields(f.field("user", d.user),          //
                              f.field("database", d.database),  //
                              f.field("tid", d.tid),            //
                              f.field("status", d.status),      //
                              f.field("collections", d.collections));
  }
};

struct TransactionActivity
    : activities::GuardedActivity<TransactionActivity,
                                  TransactionActivityData> {
  TransactionActivity(activities::ActivityId id,
                      activities::ActivityHandle parent)
      : activities::GuardedActivity<TransactionActivity,
                                    TransactionActivityData>(
            id, parent, "TransactionActivity", {}) {}
  using Data = TransactionActivityData;

  auto setTransactionId(TransactionId tid) {
    _data.getLockedGuard()->tid = tid;
  }
  auto setStatus(Status status) { _data.getLockedGuard()->status = status; }
  auto setUsername(std::string user) { _data.getLockedGuard()->user = user; }
  auto setDatabase(std::string database) {
    _data.getLockedGuard()->database = database;
  }
  auto addCollection(std::string name, DataSourceId id,
                     AccessMode::Type accessType) {
    _data.getLockedGuard()->collections.emplace_back(
        TransactionCollection{.name = name,              //
                              .cid = id,                 //
                              .accessType = accessType,  //
                              .lockStatus = LockStatus::NOT_HOLDING});
  }
  auto setLockStatus(DataSourceId id, LockStatus status) {
    auto g = _data.getLockedGuard();
    for (auto&& c : g->collections) {
      if (c.cid == id) {
        c.lockStatus = status;
        break;
      }
    }
  }
};
}  // namespace arangodb::transaction::activity
