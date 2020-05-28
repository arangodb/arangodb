////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterRepairOperations.h"
#include <utility>
#include <ctime>

#include "ServerState.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::cluster_repairs;

bool VersionSort::operator()(std::string const& a, std::string const& b) const {
  std::vector<CharOrInt> va = splitVersion(a);
  std::vector<CharOrInt> vb = splitVersion(b);

  auto compareResult =
      std::lexicographical_compare(va.begin(), va.end(), vb.begin(), vb.end(),
                                   [](CharOrInt const& a, CharOrInt const& b) -> bool {
                                     if (a.index() != b.index()) {
                                       return a.index() < b.index();
                                     }
                                     return a < b;
                                   });

  return compareResult;
}

std::vector<VersionSort::CharOrInt> VersionSort::splitVersion(std::string const& str) {
  size_t from = std::string::npos;
  size_t to = std::string::npos;

  std::vector<CharOrInt> result;

  auto addChar = [&result](char c) { result.emplace_back(c); };
  auto addUInt = [&result, &str](size_t from, size_t to) {
    result.emplace_back(WrappedUInt64{std::stoul(str.substr(from, to))});
  };

  for (size_t pos = 0; pos < str.length(); pos++) {
    if (str[pos] >= '0' && str[pos] <= '9') {
      if (from == std::string::npos) {
        from = pos;
      }
      to = pos;
    } else {
      if (to != std::string::npos) {
        addUInt(from, to);
        from = to = std::string::npos;
      }

      addChar(str[pos]);
    }
  }

  if (to != std::string::npos) {
    addUInt(from, to);
  }

  return result;
}

bool cluster_repairs::operator==(BeginRepairsOperation const& left,
                                 BeginRepairsOperation const& right) {
  return left.database == right.database && left.collectionId == right.collectionId &&
         left.collectionName == right.collectionName &&
         left.protoCollectionId == right.protoCollectionId &&
         left.protoCollectionName == right.protoCollectionName &&
         left.collectionReplicationFactor == right.collectionReplicationFactor &&
         left.protoReplicationFactor == right.protoReplicationFactor &&
         left.renameDistributeShardsLike == right.renameDistributeShardsLike;
}

bool cluster_repairs::operator==(FinishRepairsOperation const& left,
                                 FinishRepairsOperation const& right) {
  return left.database == right.database && left.collectionId == right.collectionId &&
         left.collectionName == right.collectionName &&
         left.protoCollectionId == right.protoCollectionId &&
         left.protoCollectionName == right.protoCollectionName &&
         left.shards == right.shards && left.replicationFactor == right.replicationFactor;
}

bool cluster_repairs::operator==(MoveShardOperation const& left,
                                 MoveShardOperation const& right) {
  return left.database == right.database && left.collectionId == right.collectionId &&
         left.collectionName == right.collectionName &&
         left.shard == right.shard && left.from == right.from &&
         left.to == right.to && left.isLeader == right.isLeader;
}

bool cluster_repairs::operator==(FixServerOrderOperation const& left,
                                 FixServerOrderOperation const& right) {
  return left.database == right.database && left.collectionId == right.collectionId &&
         left.collectionName == right.collectionName &&
         left.protoCollectionId == right.protoCollectionId &&
         left.protoCollectionName == right.protoCollectionName &&
         left.shard == right.shard && left.protoShard == right.protoShard &&
         left.leader == right.leader && left.followers == right.followers &&
         left.protoFollowers == right.protoFollowers;
}

std::ostream& cluster_repairs::operator<<(std::ostream& ostream,
                                          BeginRepairsOperation const& operation) {
  ostream << "BeginRepairsOperation" << std::endl
          << "{ database: " << operation.database << std::endl
          << ", collection: " << operation.collectionName << " ("
          << operation.collectionId << ")" << std::endl
          << ", protoCollection: " << operation.protoCollectionName << " ("
          << operation.protoCollectionId << ")" << std::endl
          << ", collectionReplicationFactor: " << operation.collectionReplicationFactor
          << std::endl
          << ", protoReplicationFactor: " << operation.protoReplicationFactor << std::endl
          << ", renameDistributeShardsLike: " << operation.renameDistributeShardsLike
          << std::endl
          << "}";

  return ostream;
}

std::ostream& cluster_repairs::operator<<(std::ostream& ostream,
                                          FinishRepairsOperation const& operation) {
  auto printShard = [](std::ostream& ostream,
                       ShardWithProtoAndDbServers shardWithProtoAndDbServers) {
    ShardID shardId, protoShardId;
    DBServers dbServers;
    std::tie(shardId, protoShardId, dbServers) = shardWithProtoAndDbServers;

    ostream << "{ ";
    ostream << "shard: " << shardId << ", ";
    ostream << "protoShard: " << protoShardId << ", ";
    ostream << "dbServers: ";

    if (dbServers.empty()) {
      ostream << "[]";
      return;
    } else {
      auto it = dbServers.begin();
      ostream << "[" << *it;
      ++it;

      for (; it != dbServers.end(); ++it) {
        ostream << ", " << *it;
      }

      ostream << "]";
    }
    ostream << "}";
  };

  auto printShards = [&printShard](std::ostream& ostream,
                                   std::vector<ShardWithProtoAndDbServers> shards) {
    if (shards.empty()) {
      ostream << "  []";
      return;
    }

    auto it = shards.begin();
    ostream << "  [ ";
    printShard(ostream, *it);
    ostream << "\n";
    ++it;

    for (; it != shards.end(); ++it) {
      ostream << "  , ";
      printShard(ostream, *it);
      ostream << "\n";
    }

    ostream << "  ]";
  };

  ostream << "FinishRepairsOperation" << std::endl
          << "{ database: " << operation.database << std::endl
          << ", collection: " << operation.collectionName << " ("
          << operation.collectionId << ")" << std::endl
          << ", protoCollection: " << operation.protoCollectionName << " ("
          << operation.protoCollectionId << ")" << std::endl;

  ostream << ", shards: \n";
  printShards(ostream, operation.shards);
  ostream << std::endl;

  ostream << ", replicationFactor: " << operation.replicationFactor << std::endl
          << "}";

  return ostream;
}

std::ostream& cluster_repairs::operator<<(std::ostream& ostream,
                                          MoveShardOperation const& operation) {
  ostream << "MoveShardOperation" << std::endl
          << "{ database: " << operation.database << std::endl
          << ", collection: " << operation.collectionName << " ("
          << operation.collectionId << ")" << std::endl
          << ", shard: " << operation.shard << std::endl
          << ", from: " << operation.from << std::endl
          << ", to: " << operation.to << std::endl
          << ", isLeader: " << operation.isLeader << std::endl
          << "}";

  return ostream;
}

std::ostream& cluster_repairs::operator<<(std::ostream& ostream,
                                          FixServerOrderOperation const& operation) {
  ostream << "FixServerOrderOperation" << std::endl
          << "{ database: " << operation.database << std::endl
          << ", collection: " << operation.collectionName << " ("
          << operation.collectionId << ")" << std::endl
          << ", protoCollection: " << operation.protoCollectionName << " ("
          << operation.protoCollectionId << ")" << std::endl
          << ", shard: " << operation.shard << std::endl
          << ", protoShard: " << operation.protoShard << std::endl
          << ", leader: " << operation.leader << std::endl
          << ", followers: [ ";
  for (auto const& it : operation.followers) {
    ostream << it << ", ";
  }
  ostream << "]" << std::endl;

  ostream << ", protoFollowers: [ ";
  for (auto const& it : operation.protoFollowers) {
    ostream << it << ", ";
  }
  ostream << "]" << std::endl << "}";

  return ostream;
}

class StreamRepairOperationVisitor {
 public:
  StreamRepairOperationVisitor() = delete;

  explicit StreamRepairOperationVisitor(std::ostream& stream_)
      : _stream(stream_) {}

  std::ostream& operator()(BeginRepairsOperation const& op) {
    return _stream << op;
  }
  std::ostream& operator()(FinishRepairsOperation const& op) {
    return _stream << op;
  }
  std::ostream& operator()(MoveShardOperation const& op) {
    return _stream << op;
  }
  std::ostream& operator()(FixServerOrderOperation const& op) {
    return _stream << op;
  }

 private:
  std::ostream& _stream;
};

std::ostream& cluster_repairs::operator<<(std::ostream& ostream,
                                          RepairOperation const& operation) {
  auto visitor = StreamRepairOperationVisitor(ostream);
  return std::visit(visitor, operation);
}

std::string getExtendedIsoString(std::chrono::system_clock::time_point time_point) {
  std::time_t time_t = std::chrono::system_clock::to_time_t(time_point);
  char const* format = "%FT%TZ";
  char timeString[100];
  size_t bytesWritten =
      std::strftime(timeString, sizeof(timeString), format, std::gmtime(&time_t));

  TRI_ASSERT(bytesWritten > 0);
  if (bytesWritten == 0) {
    // This should never happen
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "strftime returned 0 - buffer too small?");
  }

  return std::string(timeString);
}

class RepairOperationTypeStringVisitor {
 public:
  std::string operator()(BeginRepairsOperation const& op) const {
    return "BeginRepairsOperation";
  }
  std::string operator()(FinishRepairsOperation const& op) const {
    return "FinishRepairsOperation";
  }
  std::string operator()(MoveShardOperation const& op) const {
    return "MoveShardOperation";
  }
  std::string operator()(FixServerOrderOperation const& op) const {
    return "FixServerOrderOperation";
  }
};

std::string cluster_repairs::getTypeAsString(RepairOperation const& op) {
  return std::visit(RepairOperationTypeStringVisitor(), op);
}

VPackBufferPtr MoveShardOperation::toVPackTodo(
    uint64_t jobId, std::chrono::system_clock::time_point jobCreationTimestamp) const {
  std::string const serverId = ServerState::instance()->getId();

  std::string const isoTimeString = getExtendedIsoString(jobCreationTimestamp);

  VPackBuilder builder;

  builder.add(VPackValue(VPackValueType::Object));

  builder.add("type", VPackValue("moveShard"));
  builder.add("database", VPackValue(database));
  builder.add("collection", VPackValue(collectionId));
  builder.add("shard", VPackValue(shard));
  builder.add("fromServer", VPackValue(from));
  builder.add("toServer", VPackValue(to));
  builder.add("jobId", VPackValue(std::to_string(jobId)));
  builder.add("timeCreated", VPackValue(isoTimeString));
  builder.add("creator", VPackValue(serverId));
  builder.add("isLeader", VPackValue(isLeader));

  builder.close();

  return builder.steal();
}

template <typename T>
VPackSlice RepairOperationToTransactionVisitor::createSingleValueVPack(T val) {
  VPackBuilder builder;
  builder.add(VPackValue(val));
  VPackSlice slice = builder.slice();
  _vpackBufferArray.emplace_back(builder.steal());
  return slice;
};

RepairOperationToTransactionVisitor::ReturnValueT RepairOperationToTransactionVisitor::operator()(
    BeginRepairsOperation const& op) {
  std::string const distributeShardsLikePath =
      agencyCollectionId(op.database, op.collectionId) + "/" +
      "distributeShardsLike";
  std::string const repairingDistributeShardsLikePath =
      agencyCollectionId(op.database, op.collectionId) + "/" +
      "repairingDistributeShardsLike";
  std::string const replicationFactorPath =
      agencyCollectionId(op.database, op.collectionId) + "/" +
      "replicationFactor";
  std::string const protoReplicationFactorPath =
      agencyCollectionId(op.database, op.protoCollectionId) + "/" +
      "replicationFactor";

  VPackSlice protoCollectionIdSlice = createSingleValueVPack(op.protoCollectionId);
  VPackSlice collectionReplicationFactorSlice =
      createSingleValueVPack(op.collectionReplicationFactor);
  VPackSlice protoReplicationFactorSlice = createSingleValueVPack(op.protoReplicationFactor);

  std::vector<AgencyPrecondition> preconditions;
  std::vector<AgencyOperation> operations;

  if (op.renameDistributeShardsLike) {
    // assert that distributeShardsLike is set, but
    // repairingDistributeShardsLike
    // is not
    preconditions.emplace_back(AgencyPrecondition{distributeShardsLikePath,
                                                  AgencyPrecondition::Type::VALUE,
                                                  protoCollectionIdSlice});
    preconditions.emplace_back(AgencyPrecondition{repairingDistributeShardsLikePath,
                                                  AgencyPrecondition::Type::EMPTY, true});

    // rename distributeShardsLike to repairingDistributeShardsLike
    operations.emplace_back(AgencyOperation{repairingDistributeShardsLikePath,
                                            AgencyValueOperationType::SET,
                                            protoCollectionIdSlice});
    operations.emplace_back(AgencyOperation{distributeShardsLikePath,
                                            AgencySimpleOperationType::DELETE_OP});

    // assert replicationFactors
    preconditions.emplace_back(
        AgencyPrecondition{replicationFactorPath, AgencyPrecondition::Type::VALUE,
                           collectionReplicationFactorSlice});
    preconditions.emplace_back(AgencyPrecondition{protoReplicationFactorPath,
                                                  AgencyPrecondition::Type::VALUE,
                                                  protoReplicationFactorSlice});

    // set collection.replicationFactor = proto.replicationFactor
    operations.emplace_back(AgencyOperation{replicationFactorPath, AgencyValueOperationType::SET,
                                            protoReplicationFactorSlice});
  } else {
    // assert that repairingDistributeShardsLike is set, but
    // distributeShardsLike
    // is not
    preconditions.emplace_back(AgencyPrecondition{repairingDistributeShardsLikePath,
                                                  AgencyPrecondition::Type::VALUE,
                                                  protoCollectionIdSlice});
    preconditions.emplace_back(AgencyPrecondition{distributeShardsLikePath,
                                                  AgencyPrecondition::Type::EMPTY, true});

    // assert replicationFactors to match
    preconditions.emplace_back(AgencyPrecondition{replicationFactorPath,
                                                  AgencyPrecondition::Type::VALUE,
                                                  protoReplicationFactorSlice});
    preconditions.emplace_back(AgencyPrecondition{protoReplicationFactorPath,
                                                  AgencyPrecondition::Type::VALUE,
                                                  protoReplicationFactorSlice});
  }

  operations.emplace_back(
      AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP));

  return {AgencyWriteTransaction{operations, preconditions}, std::nullopt};
}

RepairOperationToTransactionVisitor::ReturnValueT RepairOperationToTransactionVisitor::operator()(
    FinishRepairsOperation const& op) {
  std::string const oldAttrPath = agencyCollectionId(op.database, op.collectionId) +
                                  "/" + "repairingDistributeShardsLike";
  std::string const newAttrPath = agencyCollectionId(op.database, op.collectionId) +
                                  "/" + "distributeShardsLike";
  std::string const replicationFactorPath =
      agencyCollectionId(op.database, op.collectionId) + "/" +
      "replicationFactor";
  std::string const protoReplicationFactorPath =
      agencyCollectionId(op.database, op.protoCollectionId) + "/" +
      "replicationFactor";

  VPackSlice protoCollectionIdSlice = createSingleValueVPack(op.protoCollectionId);
  VPackSlice replicationFactorSlice = createSingleValueVPack(op.replicationFactor);

  std::vector<AgencyPrecondition> preconditions{
      AgencyPrecondition{oldAttrPath, AgencyPrecondition::Type::VALUE, protoCollectionIdSlice},
      AgencyPrecondition{newAttrPath, AgencyPrecondition::Type::EMPTY, true},
      AgencyPrecondition{replicationFactorPath, AgencyPrecondition::Type::VALUE, replicationFactorSlice},
      AgencyPrecondition{protoReplicationFactorPath,
                         AgencyPrecondition::Type::VALUE, replicationFactorSlice}};

  for (auto const& it : op.shards) {
    ShardID shardId, protoShardId;
    DBServers dbServers;
    std::tie(shardId, protoShardId, dbServers) = it;
    std::string const shardPath =
        agencyCollectionId(op.database, op.collectionId) + "/shards/" + shardId;
    std::string const protoShardPath =
        agencyCollectionId(op.database, op.protoCollectionId) + "/shards/" + protoShardId;

    VPackSlice dbServersSlice;
    {
      VPackBuilder dbServerBuilder;
      dbServerBuilder.openArray();
      for (auto const& dbServer : dbServers) {
        dbServerBuilder.add(VPackValue(dbServer));
      }
      dbServerBuilder.close();

      dbServersSlice = dbServerBuilder.slice();
      _vpackBufferArray.emplace_back(dbServerBuilder.steal());
    }

    preconditions.emplace_back(
        AgencyPrecondition{shardPath, AgencyPrecondition::Type::VALUE, dbServersSlice});
    preconditions.emplace_back(
        AgencyPrecondition{protoShardPath, AgencyPrecondition::Type::VALUE, dbServersSlice});
  }

  std::vector<AgencyOperation> operations{
      AgencyOperation{newAttrPath, AgencyValueOperationType::SET, protoCollectionIdSlice},
      AgencyOperation{oldAttrPath, AgencySimpleOperationType::DELETE_OP}};

  operations.emplace_back(
      AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP));

  return {AgencyWriteTransaction{operations, preconditions}, std::nullopt};
}

RepairOperationToTransactionVisitor::ReturnValueT RepairOperationToTransactionVisitor::operator()(
    MoveShardOperation const& op) {
  uint64_t jobId = _getJobId();
  std::chrono::system_clock::time_point jobCreationTimestamp = _getJobCreationTimestamp();

  VPackBufferPtr vpackTodo = op.toVPackTodo(jobId, jobCreationTimestamp);

  _vpackBufferArray.push_back(vpackTodo);

  std::string const agencyKey = "Target/ToDo/" + std::to_string(jobId);

  return std::make_pair(
      AgencyWriteTransaction{AgencyOperation{agencyKey, AgencyValueOperationType::SET,
                                             VPackSlice(vpackTodo->data())},
                             AgencyPrecondition{agencyKey, AgencyPrecondition::Type::EMPTY, true}},
      jobId);
}

RepairOperationToTransactionVisitor::ReturnValueT RepairOperationToTransactionVisitor::operator()(
    FixServerOrderOperation const& op) {
  std::string const agencyShardId =
      agencyCollectionId(op.database, op.collectionId) + "/shards/" + op.shard;
  std::string const agencyProtoShardId =
      agencyCollectionId(op.database, op.protoCollectionId) + "/shards/" + op.protoShard;

  VPackBufferPtr vpack = createShardDbServerArray(op.leader, op.followers);
  VPackSlice oldDbServerSlice = velocypack::Slice(vpack->data());
  _vpackBufferArray.emplace_back(std::move(vpack));

  vpack = createShardDbServerArray(op.leader, op.protoFollowers);
  VPackSlice protoDbServerSlice = velocypack::Slice(vpack->data());
  _vpackBufferArray.emplace_back(std::move(vpack));

  std::vector<AgencyPrecondition> agencyPreconditions{
      AgencyPrecondition{agencyShardId, AgencyPrecondition::Type::VALUE, oldDbServerSlice},
      AgencyPrecondition{agencyProtoShardId, AgencyPrecondition::Type::VALUE, protoDbServerSlice}};

  AgencyOperation agencyOperation{agencyShardId, AgencyValueOperationType::SET,
                                  protoDbServerSlice};

  return {AgencyWriteTransaction{agencyOperation, agencyPreconditions}, std::nullopt};
}

std::string RepairOperationToTransactionVisitor::agencyCollectionId(DatabaseID database,
                                                                    CollectionID collection) const {
  return "Plan/Collections/" + database + "/" + collection;
}

VPackBufferPtr RepairOperationToTransactionVisitor::createShardDbServerArray(
    ServerID const& leader, DBServers const& followers) const {
  VPackBuilder builder;

  builder.add(velocypack::Value(velocypack::ValueType::Array));

  builder.add(velocypack::Value(leader));

  for (auto const& it : followers) {
    builder.add(velocypack::Value(it));
  }

  builder.close();

  return builder.steal();
}

RepairOperationToTransactionVisitor::RepairOperationToTransactionVisitor(ClusterInfo& ci)
    : _getJobId([&ci]() { return ci.uniqid(); }),
      _getJobCreationTimestamp([]() { return std::chrono::system_clock::now(); }) {}

RepairOperationToTransactionVisitor::RepairOperationToTransactionVisitor(
    std::function<uint64_t()> getJobId_,
    std::function<std::chrono::system_clock::time_point()> getJobCreationTimestamp_)
    : _getJobId(std::move(getJobId_)),
      _getJobCreationTimestamp(std::move(getJobCreationTimestamp_)) {}

std::vector<VPackBufferPtr>&& RepairOperationToTransactionVisitor::steal() {
  return std::move(_vpackBufferArray);
}

RepairOperationToVPackVisitor::RepairOperationToVPackVisitor(VPackBuilder& builder_)
    : _builder(builder_) {}

VPackBuilder& RepairOperationToVPackVisitor::builder() { return _builder; }

void RepairOperationToVPackVisitor::operator()(BeginRepairsOperation const& op) {
  VPackObjectBuilder outerObject(&builder());
  {
    VPackObjectBuilder innerObject(&builder(), "BeginRepairsOperation");

    builder().add("database", VPackValue(op.database));
    builder().add("collection", VPackValue(op.collectionName));
    builder().add("distributeShardsLike", VPackValue(op.protoCollectionName));
    builder().add("renameDistributeShardsLike", VPackValue(op.renameDistributeShardsLike));
    builder().add("replicationFactor", VPackValue(op.protoReplicationFactor));
  }
}

void RepairOperationToVPackVisitor::operator()(FinishRepairsOperation const& op) {
  VPackObjectBuilder outerObject(&builder());
  {
    VPackObjectBuilder innerObject(&builder(), "FinishRepairsOperation");

    builder().add("database", VPackValue(op.database));
    builder().add("collection", VPackValue(op.collectionName));
    builder().add("distributeShardsLike", VPackValue(op.protoCollectionName));

    VPackArrayBuilder shards(&builder(), "shards");
    for (auto const& it : op.shards) {
      ShardID shardId, protoShardId;
      DBServers dbServers;
      std::tie(shardId, protoShardId, dbServers) = it;
      VPackObjectBuilder shard(&builder());
      builder().add("shard", VPackValue(shardId));
      builder().add("protoShard", VPackValue(protoShardId));
      {
        VPackArrayBuilder dbServersGuard(&builder(), "dbServers");
        for (auto const& dbServer : dbServers) {
          builder().add(VPackValue(dbServer));
        }
      }
    }
  }
}

void RepairOperationToVPackVisitor::operator()(MoveShardOperation const& op) {
  VPackObjectBuilder outerObject(&builder());
  {
    VPackObjectBuilder innerObject(&builder(), "MoveShardOperation");

    builder().add("database", VPackValue(op.database));
    builder().add("collection", VPackValue(op.collectionName));
    builder().add("shard", VPackValue(op.shard));
    builder().add("from", VPackValue(op.from));
    builder().add("to", VPackValue(op.to));
    builder().add("isLeader", VPackValue(op.isLeader));
  }
}

void RepairOperationToVPackVisitor::operator()(FixServerOrderOperation const& op) {
  VPackObjectBuilder outerObject(&builder());
  {
    VPackObjectBuilder innerObject(&builder(), "FixServerOrderOperation");
    builder().add("database", VPackValue(op.database));
    builder().add("collection", VPackValue(op.collectionName));
    builder().add("distributeShardsLike", VPackValue(op.protoCollectionName));
    builder().add("shard", VPackValue(op.shard));
    builder().add("distributeShardsLikeShard", VPackValue(op.protoShard));
    builder().add("leader", VPackValue(op.leader));
    {
      VPackArrayBuilder followers(&builder(), "followers");
      for (ServerID const& follower : op.followers) {
        builder().add(VPackValue(follower));
      }
    }
    {
      VPackArrayBuilder distributeShardsLikeFollowers(
          &builder(), "distributeShardsLikeFollowers");
      for (ServerID const& protoFollower : op.protoFollowers) {
        builder().add(VPackValue(protoFollower));
      }
    }
  }
}

BeginRepairsOperation::BeginRepairsOperation(
    const tagged_argument<tag::database, DatabaseID> database_,
    const tagged_argument<tag::collectionId, CollectionID> collectionId_,
    const tagged_argument<tag::collectionName, std::string> collectionName_,
    const tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
    const tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
    const tagged_argument<tag::collectionReplicationFactor, uint64_t> collectionReplicationFactor_,
    const tagged_argument<tag::protoReplicationFactor, uint64_t> protoReplicationFactor_,
    const tagged_argument<tag::renameDistributeShardsLike, bool> renameDistributeShardsLike_)
    : database(database_.value),
      collectionId(collectionId_.value),
      collectionName(collectionName_.value),
      protoCollectionId(protoCollectionId_.value),
      protoCollectionName(protoCollectionName_.value),
      collectionReplicationFactor(collectionReplicationFactor_.value),
      protoReplicationFactor(protoReplicationFactor_.value),
      renameDistributeShardsLike(renameDistributeShardsLike_.value) {}

FinishRepairsOperation::FinishRepairsOperation(
    const tagged_argument<tag::database, DatabaseID> database_,
    const tagged_argument<tag::collectionId, CollectionID> collectionId_,
    const tagged_argument<tag::collectionName, std::string> collectionName_,
    const tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
    const tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
    const tagged_argument<tag::shards, std::vector<ShardWithProtoAndDbServers>> shards_,
    const tagged_argument<tag::replicationFactor, uint64_t> replicationFactor_)
    : database(database_.value),
      collectionId(collectionId_.value),
      collectionName(collectionName_.value),
      protoCollectionId(protoCollectionId_.value),
      protoCollectionName(protoCollectionName_.value),
      shards(shards_.value),
      replicationFactor(replicationFactor_.value) {}

MoveShardOperation::MoveShardOperation(
    const tagged_argument<tag::database, DatabaseID> database_,
    const tagged_argument<tag::collectionId, CollectionID> collectionId_,
    const tagged_argument<tag::collectionName, std::string> collectionName_,
    const tagged_argument<tag::shard, ShardID> shard_,
    const tagged_argument<tag::from, ServerID> from_,
    const tagged_argument<tag::to, ServerID> to_,
    const tagged_argument<tag::isLeader, bool> isLeader_)
    : database(database_.value),
      collectionId(collectionId_.value),
      collectionName(collectionName_.value),
      shard(shard_.value),
      from(from_.value),
      to(to_.value),
      isLeader(isLeader_.value) {}

FixServerOrderOperation::FixServerOrderOperation(
    const tagged_argument<tag::database, DatabaseID> database_,
    const tagged_argument<tag::collectionId, CollectionID> collectionId_,
    const tagged_argument<tag::collectionName, std::string> collectionName_,
    const tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
    const tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
    const tagged_argument<tag::shard, ShardID> shard_,
    const tagged_argument<tag::protoShard, ShardID> protoShard_,
    const tagged_argument<tag::leader, ServerID> leader_,
    const tagged_argument<tag::followers, std::vector<ServerID>> followers_,
    const tagged_argument<tag::protoFollowers, std::vector<ServerID>> protoFollowers_)
    : database(database_.value),
      collectionId(collectionId_.value),
      collectionName(collectionName_.value),
      protoCollectionId(protoCollectionId_.value),
      protoCollectionName(protoCollectionName_.value),
      shard(shard_.value),
      protoShard(protoShard_.value),
      leader(leader_.value),
      followers(followers_.value),
      protoFollowers(protoFollowers_.value) {}
