////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "RocksDBEngine/RocksDBPersistedLog.h"

using namespace arangodb::replication2;

struct ReplicatedLogTest2 : ::testing::Test {};

TEST_F(ReplicatedLogTest2, foo_bar) {
  // auto log = std::make_shared<replicated_log::ReplicatedLog>();
}
