#include "VocbaseInfo.h"

#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"
#include "Basics/StringUtils.h"
#include "Utils/Events.h"
#include "Logger/LogMacros.h"
#include "Cluster/ClusterFeature.h"

namespace arangodb {

  //  ID has to be created outside!!!
  /// if (ServerState::instance()->isCoordinator()) {
  ///   _id = ClusterInfo::instance()->uniqid();
  /// } else {
  ///   if (options.hasKey("id")) {
  ///     _id = basics::VelocyPackHelper::stringUInt64(options, "id");
  ///   } else {
  ///     _id = 0; // TODO -- is this ok?
  ///   }
  /// }

Result CreateDatabaseInfo::load(std::string const& name, uint64_t id) {
  Result res;

  _name = name;
  _id = id;

#ifdef  ARANGODB_ENABLE_MAINTAINER_MODE
  _vaild = true;
#endif

  return checkOptions();
}

Result CreateDatabaseInfo::load(VPackSlice const& options,
                                VPackSlice const& users) {

  Result res = extractOptions(options, true /*getId*/, true /*getUser*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef  ARANGODB_ENABLE_MAINTAINER_MODE
  _vaild = true;
#endif

  return res;
};

Result CreateDatabaseInfo::load(uint64_t id, VPackSlice const& options, VPackSlice const& users) {
  _id = id;

  Result res = extractOptions(options, false /*getId*/, true /*getUser*/);
  if (!res.ok()) {
    return res;
  }

  res = extractUsers(users);
  if (!res.ok()) {
    return res;
  }

#ifdef  ARANGODB_ENABLE_MAINTAINER_MODE
  _vaild = true;
#endif

  return res;
}

Result CreateDatabaseInfo::load(std::string const& name,
                                VPackSlice const& options,
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

#ifdef  ARANGODB_ENABLE_MAINTAINER_MODE
  _vaild = true;
#endif

  return res;
}

Result CreateDatabaseInfo::load(std::string const& name, uint64_t id, VPackSlice const& options,
                                VPackSlice const& users) {
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

#ifdef  ARANGODB_ENABLE_MAINTAINER_MODE
  _vaild = true;
#endif

  return res;
};


void CreateDatabaseInfo::toVelocyPack(VPackBuilder& builder, bool withUsers) const {
  TRI_ASSERT(_vaildId);
  TRI_ASSERT(builder.isOpenObject());
  std::string const idString(basics::StringUtils::itoa(_id));
  builder.add(StaticStrings::DatabaseId, VPackValue(idString));
  builder.add(StaticStrings::DatabaseName, VPackValue(_name));
  builder.add(StaticStrings::ReplicationFactor, VPackValue(_replicationFactor));
  builder.add(StaticStrings::MinReplicationFactor, VPackValue(_minReplicationFactor));
  builder.add(StaticStrings::Sharding, VPackValue(_sharding));

  if(withUsers) {
    builder.add(VPackValue("users"));
    UsersToVelocyPack(builder);
  }
}

void CreateDatabaseInfo::UsersToVelocyPack(VPackBuilder& builder) const {
  VPackArrayBuilder guard(&builder);
  for(auto const& user : _users) {
    builder.add("username", VPackValue(user.name));
    builder.add("passwd", VPackValue(user.password));
    builder.add("active", VPackValue(user.active));
    builder.add("extra", user.extra->slice());
  }
}

Result CreateDatabaseInfo::extractUsers(VPackSlice const& users) {
  if (users.isNone() || users.isNull()) {
    return Result();
  } else if (!users.isArray()) {
    events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid users slice");
  }

  for (VPackSlice const& user : VPackArrayIterator(users)) {
    if (!user.isObject()) {
      events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }

    std::string name;
    for(std::string const& key : std::vector<std::string>{"username", "user"}) {
      auto slice = user.get(key);
      if (slice.isNone()) {
        continue;
      } else if (slice.isString()) {
        name = slice.copyString();
      } else {
        events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
        return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
      }
    }

    std::string password;
    if (user.hasKey("passwd")) {
      VPackSlice passwd = user.get("passwd");
      if (!passwd.isString()) {
        events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
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

    // ignore empty names
    if(!name.empty()) {
      _users.emplace_back(std::move(name), std::move(password), active, std::move(extraBuilder));
    } else {
      LOG_TOPIC("da345", DEBUG, Logger::AUTHENTICATION) << "empty user name";
    }
  }
  return Result();
}

Result CreateDatabaseInfo::extractOptions(VPackSlice const& options, bool extractId, bool extractName) {
  if (options.isNone() || options.isNull()) {
    return Result();
  } else if (!options.isObject()) {
    events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid options slice");
  }

  auto vocopts = getVocbaseOptions(options);
  _replicationFactor = vocopts.replicationFactor;
  _minReplicationFactor = vocopts.minReplicationFactor;
  _sharding = vocopts.sharding;

  if(extractName) {
    auto nameSlice = options.get(StaticStrings::DatabaseName);
    if(!nameSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, "no valid id given");
    }
    _name = nameSlice.copyString();
  }

  if(extractId) {
    auto idSlice = options.get(StaticStrings::DatabaseId);
      if(idSlice.isString()){
        // improve once it works
        // look for some nice internal helper this has proably been done before
        auto idStr = idSlice.copyString();
        static_assert(std::is_same<std::uint64_t,decltype(std::stoul(std::declval<std::string>()))>::value, "bad type" );
        _id = std::stoul(idStr);

      } else if ( idSlice.isUInt()) {
        _id = idSlice.getUInt();
      } else if ( idSlice.isNone()) {
        //do nothing - can be set later - this sucks
      } else {
        return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, "no valid id given");
      }
  }

  return checkOptions();
}

Result CreateDatabaseInfo::checkOptions() {

  if(_id != 0){
    _vaildId = true;
  } else {
    _vaildId = false;
  }

  if (!TRI_vocbase_t::IsAllowedName(true /*TODO _isSystemDB*/, arangodb::velocypack::StringRef(_name))) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  return Result();
}

VocbaseOptions getVocbaseOptions(VPackSlice const& options) {
  LOG_DEVEL << "getVocbaseOptions - options" << options.toJson();
  TRI_ASSERT(options.isObject());
  // Invalid options will be silently ignored. Default values will be used
  // instead.
  //
  // This Function will be called twice - the second time we do not run in
  // the risk of consulting the ClusterFeature, because defaults are provided
  // during the first call.
  VocbaseOptions vocbaseOptions;
  vocbaseOptions.replicationFactor = 1;
  vocbaseOptions.minReplicationFactor = 1;
  vocbaseOptions.sharding = "";

  //  sanitize input for vocbase creation
  //  sharding -- must be "", "flexible" or "single"
  //  replicationFactor must be "satellite" or a natural number
  //  minReplicationFactor must be or a natural number

  {
    auto shardingSlice = options.get(StaticStrings::Sharding);
    if(! (shardingSlice.isString()  &&
          (shardingSlice.compareString("") == 0 || shardingSlice.compareString("flexible") == 0 || shardingSlice.compareString("single") == 0)
         )) {
      shardingSlice = VPackSlice::noneSlice();
    } else {
      vocbaseOptions.sharding = shardingSlice.copyString();
    }
  }

  ClusterFeature* cluster = dynamic_cast<ClusterFeature*>(application_features::ApplicationServer::lookupFeature("Cluster"));
  {
    VPackSlice replicationSlice = options.get(StaticStrings::ReplicationFactor);
    bool isSatellite = (replicationSlice.isString() && replicationSlice.compareString(StaticStrings::Satellite) == 0 );
    bool isNumber = (replicationSlice.isNumber() && replicationSlice.getUInt() > 0 );
    if(!isSatellite && !isNumber){
      if(cluster) {
        vocbaseOptions.replicationFactor = cluster->defaultReplicationFactor();
      } else {
        LOG_TOPIC("eeeee", ERR, Logger::CLUSTER) << "Can not access ClusterFeature to determine database replicationFactor";
      }
    } else if (isSatellite) {
      vocbaseOptions.replicationFactor = 0;
    } else if (isNumber) {
      vocbaseOptions.replicationFactor = replicationSlice.getNumber<decltype(vocbaseOptions.replicationFactor)>();
    }
#ifndef USE_ENTERPRISE
    if(vocbaseOptions.replicationFactor == 0) {
      if(cluster) {
        vocbaseOptions.replicationFactor = cluster->defaultReplicationFactor();
      } else {
        LOG_TOPIC("eeeef", ERR, Logger::CLUSTER) << "Can not access ClusterFeature to determine database replicationFactor";
        vocbaseOptions.replicationFactor = 1;
      }
    }
#endif
  }

  {
    VPackSlice minReplicationSlice = options.get(StaticStrings::MinReplicationFactor);
    bool isNumber = (minReplicationSlice.isNumber() && minReplicationSlice.getNumber<int>() > 0 );
    if(!isNumber){
      if(cluster) {
        vocbaseOptions.minReplicationFactor = cluster->minReplicationFactor();
      } else {
        LOG_TOPIC("eeeed", ERR, Logger::CLUSTER) << "Can not access ClusterFeature to determine database minReplicationFactor";
      }
    } else if (isNumber) {
      vocbaseOptions.minReplicationFactor = minReplicationSlice.getNumber<decltype(vocbaseOptions.replicationFactor)>();
    }
  }

  return vocbaseOptions;
}

void addVocbaseOptionsToOpenObject(VPackBuilder& builder, std::string const& sharding, std::uint32_t replicationFactor, std::uint32_t minReplicationFactor) {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(StaticStrings::Sharding, VPackValue(sharding));
  if(replicationFactor) {
    builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
  } else { // 0 is satellite
    builder.add(StaticStrings::ReplicationFactor, VPackValue(StaticStrings::Satellite));
  }
  builder.add(StaticStrings::MinReplicationFactor, VPackValue(minReplicationFactor));
}

void addVocbaseOptionsToOpenObject(VPackBuilder& builder, VocbaseOptions const& opt) {
  addVocbaseOptionsToOpenObject(builder, opt.sharding, opt.replicationFactor, opt.minReplicationFactor);
}
}
