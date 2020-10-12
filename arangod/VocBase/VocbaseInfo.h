////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_VOCBASEINFO_H
#define ARANGOD_VOCBASE_VOCBASEINFO_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Utils/OperationOptions.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

// TODO do we need to add some sort of coordinator?
// builder.add("coordinator", VPackValue(ServerState::instance()->getId()));

struct DBUser {
  DBUser() = default;
  DBUser(DBUser const&) = default;  // delete when info does not need to be copied anymore

  DBUser(std::string&& n, std::string&& p, bool a, std::shared_ptr<VPackBuilder> b)
      : name(std::move(n)), password(std::move(p)), extra(std::move(b)), active(a) {}

  DBUser(std::string const& n, std::string const& p, bool a, std::shared_ptr<VPackBuilder> b)
      : name(n), password(p), extra(std::move(b)), active(a) {}

  DBUser& operator=(DBUser&& other) {
    name = std::move(other.name);
    password = std::move(other.password);
    active = other.active;
    extra = std::move(other.extra);
    return *this;
  }

  DBUser(DBUser&& other) { operator=(std::move(other)); }

  std::string name;
  std::string password;
  std::shared_ptr<VPackBuilder> extra;  // TODO - must be unique_ptr eventually
  bool active = false;
};

class CreateDatabaseInfo {
 public:
  CreateDatabaseInfo(application_features::ApplicationServer&, ExecContext const&);
  Result load(std::string const& name, uint64_t id);

  Result load(uint64_t id, VPackSlice const& options,
              VPackSlice const& users = VPackSlice::emptyArraySlice());

  Result load(std::string const& name, VPackSlice const& options,
              VPackSlice const& users = VPackSlice::emptyArraySlice());

  Result load(std::string const& name, uint64_t id, VPackSlice const& options,
              VPackSlice const& users);

  Result load(VPackSlice const& options, VPackSlice const& users);

  void toVelocyPack(VPackBuilder& builder, bool withUsers = false) const;
  void UsersToVelocyPack(VPackBuilder& builder) const;

  application_features::ApplicationServer& server() const;

  uint64_t getId() const {
    TRI_ASSERT(_valid);
    TRI_ASSERT(_validId);
    return _id;
  }

  bool valid() const { return _valid; }

  bool validId() const { return _validId; }

  // shold be created with vaild id
  void setId(uint64_t id) {
    _id = id;
    _validId = true;
  }

  std::string const& getName() const {
    TRI_ASSERT(_valid);
    return _name;
  }

  std::uint32_t replicationFactor() const {
    TRI_ASSERT(_valid);
    return _replicationFactor;
  }

  std::uint32_t writeConcern() const {
    TRI_ASSERT(_valid);
    return _writeConcern;
  }
  std::string const& sharding() const {
    TRI_ASSERT(_valid);
    return _sharding;
  }

  ShardingPrototype shardingPrototype() const;
  void shardingPrototype(ShardingPrototype type);

 private:
  Result extractUsers(VPackSlice const& users);
  Result extractOptions(VPackSlice const& options, bool extactId = true,
                        bool extractName = true);
  Result checkOptions();

 private:
  application_features::ApplicationServer& _server;
  ExecContext const& _context;

  std::uint64_t _id = 0;
  std::string _name = "";
  std::string _sharding = "flexible";
  std::vector<DBUser> _users;

  std::uint32_t _replicationFactor = 1;
  std::uint32_t _writeConcern = 1;
  ShardingPrototype _shardingPrototype = ShardingPrototype::Undefined;

  bool _validId = false;
  bool _valid = false;  // required because TRI_ASSERT needs variable in Release mode.
};

struct VocbaseOptions {
  std::string sharding = "";
  std::uint32_t replicationFactor = 1;
  std::uint32_t writeConcern = 1;
};

VocbaseOptions getVocbaseOptions(application_features::ApplicationServer&, velocypack::Slice const&);

void addClusterOptions(velocypack::Builder& builder, std::string const& sharding,
                                   std::uint32_t replicationFactor,
                                   std::uint32_t writeConcern);
void addClusterOptions(velocypack::Builder&, VocbaseOptions const&);

}  // namespace arangodb
#endif
