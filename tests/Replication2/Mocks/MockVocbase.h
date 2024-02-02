////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Utils/VersionTracker.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocbaseInfo.h"
#include "Mocks/StorageEngineMock.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb::replication2::tests {

struct MockCreateDatabaseInfo : CreateDatabaseInfo {
  MockCreateDatabaseInfo(ArangodServer& server, ExecContext const& execContext,
                         std::string const& name, std::uint64_t id)
      : CreateDatabaseInfo(CreateDatabaseInfo::mockConstruct, server,
                           execContext, name, id, replication::Version::TWO) {}

  virtual ~MockCreateDatabaseInfo() = default;
};

struct MockVocbase : TRI_vocbase_t {
  static auto createDatabaseInfo(ArangodServer& server, std::string const& name,
                                 std::uint64_t id) -> MockCreateDatabaseInfo {
    return MockCreateDatabaseInfo(server, ExecContext::current(), name, id);
  }

  explicit MockVocbase(ArangodServer& server, std::string const& name,
                       std::uint64_t id)
      : TRI_vocbase_t(TRI_vocbase_t::mockConstruct,
                      createDatabaseInfo(server, name, id), storageEngine,
                      versionTracker, true),
        storageEngine(server) {
    server.addFeature<arangodb::EngineSelectorFeature>();
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(
        &storageEngine);
  }

  virtual ~MockVocbase() = default;

  auto registerLogicalCollection(std::string name, LogId logId) {
    VPackBuilder builder;
    builder.openObject();
    builder.add("name", std::move(name));
    builder.add("groupId", 1234);
    builder.add("replicatedStateId", logId);
    builder.close();
    auto col =
        std::make_shared<LogicalCollection>(*this, builder.slice(), false);
    storageEngine.registerCollection(*this, col);
    return col;
  }

  StorageEngineMock storageEngine;
  VersionTracker versionTracker;
};

}  // namespace arangodb::replication2::tests
