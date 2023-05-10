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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "RestServer/arangod.h"
#include "Replication2/Version.h"
#include "Utils/OperationOptions.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

struct DBUser {
  DBUser() = default;
  DBUser(DBUser const&) =
      default;  // delete when info does not need to be copied anymore

  DBUser(std::string&& n, std::string&& p, bool a,
         std::shared_ptr<VPackBuilder> b)
      : name(std::move(n)),
        password(std::move(p)),
        extra(std::move(b)),
        active(a) {}

  DBUser(std::string const& n, std::string const& p, bool a,
         std::shared_ptr<VPackBuilder> b)
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
  CreateDatabaseInfo(ArangodServer&, ExecContext const&);
  Result load(std::string_view name, uint64_t id);

  Result load(std::string_view name, VPackSlice options,
              VPackSlice users = VPackSlice::emptyArraySlice());

  Result load(std::string_view name, uint64_t id, VPackSlice options,
              VPackSlice users);

  Result load(VPackSlice options, VPackSlice users);

  void toVelocyPack(VPackBuilder& builder, bool withUsers = false) const;
  void UsersToVelocyPack(VPackBuilder& builder) const;

  ArangodServer& server() const;

  uint64_t getId() const;

  void strictValidation(bool value) noexcept { _strictValidation = value; }

  bool valid() const noexcept { return _valid; }

  bool validId() const noexcept { return _validId; }

  // shold be created with valid id
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

  [[nodiscard]] replication::Version replicationVersion() const {
    TRI_ASSERT(_valid);
    return _replicationVersion;
  }

  std::string const& sharding() const {
    TRI_ASSERT(_valid);
    return _sharding;
  }

  void sharding(std::string const& sharding) { _sharding = sharding; }

  ShardingPrototype shardingPrototype() const;
  void shardingPrototype(ShardingPrototype type);

#ifdef ARANGODB_USE_GOOGLE_TESTS
 protected:
  struct MockConstruct {
  } constexpr static mockConstruct = {};
  CreateDatabaseInfo(MockConstruct, ArangodServer& server,
                     ExecContext const& execContext, std::string const& name,
                     std::uint64_t id);
#endif

 private:
  Result extractUsers(VPackSlice users);
  Result extractOptions(VPackSlice options, bool extactId = true,
                        bool extractName = true);
  Result checkOptions();

 private:
  ArangodServer& _server;
  ExecContext const& _context;

  std::uint64_t _id = 0;
  std::string _name = "";
  std::string _sharding = "flexible";
  std::vector<DBUser> _users;

  std::uint32_t _replicationFactor = 1;
  std::uint32_t _writeConcern = 1;
  replication::Version _replicationVersion = replication::Version::ONE;
  ShardingPrototype _shardingPrototype = ShardingPrototype::Undefined;

  bool _strictValidation = true;
  bool _validId = false;
  bool _valid = false;
};

struct VocbaseOptions {
  std::string sharding;
  std::uint32_t replicationFactor = 1;
  std::uint32_t writeConcern = 1;
  replication::Version replicationVersion = replication::Version::ONE;
};

VocbaseOptions getVocbaseOptions(ArangodServer&, velocypack::Slice,
                                 bool strictValidation);

void addClusterOptions(VPackBuilder& builder, std::string const& sharding,
                       std::uint32_t replicationFactor,
                       std::uint32_t writeConcern,
                       replication::Version replicationVersion);
void addClusterOptions(velocypack::Builder&, VocbaseOptions const&);

}  // namespace arangodb
