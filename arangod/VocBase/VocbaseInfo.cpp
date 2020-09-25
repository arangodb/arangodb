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

#include "VocbaseInfo.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Utils/Events.h"

namespace arangodb {

CreateDatabaseInfo::CreateDatabaseInfo(application_features::ApplicationServer& server,
                                       ExecContext const& context)
    : _server(server), _context(context) {}

ShardingPrototype CreateDatabaseInfo::shardingPrototype() const { 
  if (_name != StaticStrings::SystemDatabase) {
    return ShardingPrototype::Graphs;
  }
  return _shardingPrototype; 
}

void CreateDatabaseInfo::shardingPrototype(ShardingPrototype type) {
  _shardingPrototype = type; 
}

Result CreateDatabaseInfo::load(std::string const& name, uint64_t id) {
  Result res;

  _name = name;
  _id = id;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _valid = true;
#endif 
  return checkOptions();
}

Result CreateDatabaseInfo::load(VPackSlice const& options, VPackSlice const& users) {
  Result res = extractOptions(options, true /*getId*/, true /*getUser*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _valid = true;
#endif

  return checkOptions();
}

Result CreateDatabaseInfo::load(uint64_t id, VPackSlice const& options,
                                VPackSlice const& users) {
  _id = id;

  Result res = extractOptions(options, false /*getId*/, true /*getUser*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _valid = true;
#endif

  return checkOptions();
}

Result CreateDatabaseInfo::load(std::string const& name, VPackSlice const& options,
                                VPackSlice const& users) {
  _name = name;

  Result res = extractOptions(options, true /*getId*/, false /*getName*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _valid = true;
#endif

  return checkOptions();
}

Result CreateDatabaseInfo::load(std::string const& name, uint64_t id,
                                VPackSlice const& options, VPackSlice const& users) {
  _name = name;
  _id = id;

  Result res = extractOptions(options, false /*getId*/, false /*getUser*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _valid = true;
#endif

  return checkOptions();
}

void CreateDatabaseInfo::toVelocyPack(VPackBuilder& builder, bool withUsers) const {
  TRI_ASSERT(_validId);
  TRI_ASSERT(builder.isOpenObject());
  std::string const idString(basics::StringUtils::itoa(_id));
  builder.add(StaticStrings::DatabaseId, VPackValue(idString));
  builder.add(StaticStrings::DatabaseName, VPackValue(_name));
  builder.add(StaticStrings::DataSourceSystem, VPackValue(_name == StaticStrings::SystemDatabase));

  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isDBServer()) {
    addClusterOptions(builder, _sharding, _replicationFactor, _writeConcern);
  } 

  if (withUsers) {
    builder.add(VPackValue("users"));
    UsersToVelocyPack(builder);
  }
}

void CreateDatabaseInfo::UsersToVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder arrayGuard(&builder);
  for (auto const& user : _users) {
    VPackObjectBuilder objectGuard(&builder);
    builder.add("username", VPackValue(user.name));
    builder.add("passwd", VPackValue(user.password));
    builder.add("active", VPackValue(user.active));
    if (user.extra) {
      builder.add("extra", user.extra->slice());
    }
  }
}

application_features::ApplicationServer& CreateDatabaseInfo::server() const { return _server; }

Result CreateDatabaseInfo::extractUsers(VPackSlice const& users) {
  if (users.isNone() || users.isNull()) {
    return Result();
  } else if (!users.isArray()) {
    events::CreateDatabase(_name, Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid users slice");
  }

  for (VPackSlice const& user : VPackArrayIterator(users)) {
    if (!user.isObject()) {
      events::CreateDatabase(_name, Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }

    std::string name;
    bool userSet = false;
    for (std::string const& key :
         std::vector<std::string>{"username", "user"}) {
      auto slice = user.get(key);
      if (slice.isNone()) {
        continue;
      } else if (slice.isString()) {
        name = slice.copyString();
        userSet = true;
      } else {
        events::CreateDatabase(_name, Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
        return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
      }
    }

    std::string password;
    if (user.hasKey("passwd")) {
      VPackSlice passwd = user.get("passwd");
      if (!passwd.isString()) {
        events::CreateDatabase(_name, Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
        return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
      }
      password = passwd.copyString();
    }

    bool active = true;
    VPackSlice act = user.get("active");
    if (act.isBool()) {
      active = act.getBool();
    }

    std::shared_ptr<VPackBuilder> extraBuilder;
    VPackSlice extra = user.get("extra");
    if (extra.isObject()) {
      extraBuilder = std::make_shared<VPackBuilder>();
      extraBuilder->add(extra);
    }

    if (userSet) {
      _users.emplace_back(std::move(name), std::move(password), active,
                          std::move(extraBuilder));
    } else {
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }
  }
  return Result();
}

Result CreateDatabaseInfo::extractOptions(VPackSlice const& options,
                                          bool extractId, bool extractName) {
  if (options.isNone() || options.isNull()) {
    return Result();
  }
  if (!options.isObject()) {
    events::CreateDatabase(_name, Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid options slice");
  }

  auto vocopts = getVocbaseOptions(_server, options);
  _replicationFactor = vocopts.replicationFactor;
  _writeConcern = vocopts.writeConcern;
  _sharding = vocopts.sharding;

  if (extractName) {
    auto nameSlice = options.get(StaticStrings::DatabaseName);
    if (!nameSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, "no valid id given");
    }
    _name = nameSlice.copyString();
  }
  if (extractId) {
    auto idSlice = options.get(StaticStrings::DatabaseId);
    if (idSlice.isString()) {
      // improve once it works
      // look for some nice internal helper this has proably been done before
      auto idStr = idSlice.copyString();
      _id = basics::StringUtils::uint64(idSlice.stringRef().data(),
                                        idSlice.stringRef().size());

    } else if (idSlice.isUInt()) {
      _id = idSlice.getUInt();
    } else if (idSlice.isNone()) {
      // do nothing - can be set later - this sucks
    } else {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, "no valid id given");
    }
  }

  return checkOptions();
}

Result CreateDatabaseInfo::checkOptions() {
  if (_id != 0) {
    _validId = true;
  } else {
    _validId = false;
  }

  // we cannot use IsAllowedName for database name length validation alone, because
  // IsAllowedName allows up to 256 characters. Database names are just up to 64
  // chars long, as their names are also used as filesystem directories (for Foxx apps)
  bool isSystem = _name == StaticStrings::SystemDatabase;

  if (_name.empty() ||
      !TRI_vocbase_t::IsAllowedName(isSystem, arangodb::velocypack::StringRef(_name)) ||
      _name.size() > 64) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  return Result();
}

VocbaseOptions getVocbaseOptions(application_features::ApplicationServer& server, VPackSlice const& options) {
  TRI_ASSERT(options.isObject());
  // Invalid options will be silently ignored. Default values will be used
  // instead.
  //
  // This Function will be called twice - the second time we do not run in
  // the risk of consulting the ClusterFeature, because defaults are provided
  // during the first call.
  VocbaseOptions vocbaseOptions;
  vocbaseOptions.replicationFactor = 1;
  vocbaseOptions.writeConcern = 1;
  vocbaseOptions.sharding = "";

  //  sanitize input for vocbase creation
  //  sharding -- must be "", "flexible" or "single"
  //  replicationFactor must be "satellite" or a natural number
  //  writeConcern must be a natural number

  {
    auto shardingSlice = options.get(StaticStrings::Sharding);
    if (shardingSlice.isString() && shardingSlice.compareString("single") == 0) {
      vocbaseOptions.sharding = shardingSlice.copyString();
    }
  }

  bool haveCluster = server.hasFeature<ClusterFeature>();
  {
    VPackSlice replicationSlice = options.get(StaticStrings::ReplicationFactor);
    bool isSatellite = (replicationSlice.isString() &&
                        replicationSlice.compareString(StaticStrings::Satellite) == 0);
    bool isNumber = replicationSlice.isNumber();
    isSatellite = isSatellite || (isNumber && replicationSlice.getUInt() == 0);
    if (!isSatellite && !isNumber) {
      if (haveCluster) {
        vocbaseOptions.replicationFactor = server.getFeature<ClusterFeature>().defaultReplicationFactor();
      } else {
        LOG_TOPIC("eeeee", ERR, Logger::CLUSTER)
            << "Cannot access ClusterFeature to determine replicationFactor";
      }
    } else if (isSatellite) {
      vocbaseOptions.replicationFactor = 0;
    } else if (isNumber) {
      vocbaseOptions.replicationFactor =
          replicationSlice.getNumber<decltype(vocbaseOptions.replicationFactor)>();
    }
#ifndef USE_ENTERPRISE
    if (vocbaseOptions.replicationFactor == 0) {
      if (haveCluster) {
        vocbaseOptions.replicationFactor = server.getFeature<ClusterFeature>().defaultReplicationFactor();
      } else {
        LOG_TOPIC("eeeef", ERR, Logger::CLUSTER)
            << "Cannot access ClusterFeature to determine replicationFactor";
        vocbaseOptions.replicationFactor = 1;
      }
    }
#endif
  }

  {
    // simon: new API in 3.6 no need to check legacy "minReplicationFactor"
    VPackSlice writeConcernSlice = options.get(StaticStrings::WriteConcern);
    bool isNumber =
        (writeConcernSlice.isNumber() && writeConcernSlice.getNumber<int>() > 0);
    if (!isNumber) {
      if (haveCluster) {
        vocbaseOptions.writeConcern = server.getFeature<ClusterFeature>().writeConcern();
      } else {
        LOG_TOPIC("eeeed", ERR, Logger::CLUSTER)
            << "Cannot access ClusterFeature to determine writeConcern";
      }
    } else if (isNumber) {
      vocbaseOptions.writeConcern =
          writeConcernSlice.getNumber<decltype(vocbaseOptions.writeConcern)>();
    }
  }

  return vocbaseOptions;
}

void addClusterOptions(VPackBuilder& builder, std::string const& sharding,
                                   std::uint32_t replicationFactor,
                                   std::uint32_t writeConcern) {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(StaticStrings::Sharding, VPackValue(sharding));
  if (replicationFactor) {
    builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
  } else {  // 0 is satellite
    builder.add(StaticStrings::ReplicationFactor, VPackValue(StaticStrings::Satellite));
  }
  builder.add(StaticStrings::WriteConcern, VPackValue(writeConcern));
}

void addClusterOptions(VPackBuilder& builder, VocbaseOptions const& opt) {
  addClusterOptions(builder, opt.sharding, opt.replicationFactor, opt.writeConcern);
}
}  // namespace arangodb
