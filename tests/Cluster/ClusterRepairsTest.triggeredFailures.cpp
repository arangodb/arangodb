// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "10000001": {
      "name": "follower10000001of10000002",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000002": {
      "name": "prototype10000002",
      "replicationFactor": 1,
      "shards": {
        "s21": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000003": {
      "name": "follower10000003of10000002|fail_mismatching_leaders",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s31": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000004": {
      "name": "follower10000004of10000002|fail_mismatching_followers",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s41": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000005": {
      "name": "follower10000005of10000002|fail_inconsistent_attributes_in_repairDistributeShardsLike",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s51": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000006": {
      "name": "follower10000006of10000002|fail_inconsistent_attributes_in_createBeginRepairsOperation",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s61": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000007": {
      "name": "follower10000007of10000002|fail_inconsistent_attributes_in_createFinishRepairsOperation",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s71": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000098": {
      "name": "prototype10000098",
      "replicationFactor": 1,
      "shards": {
        "s981": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000099": {
      "name": "follower10000099of10000098",
      "replicationFactor": 1,
      "distributeShardsLike": "10000098",
      "shards": {
        "s991": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth2Healthy0Bad = R"=(
{
  "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2": {
  },
  "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA": {
    "Status": "GOOD"
  },
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  }
}
)="_vpack;

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
expectedResultsWithTriggeredFailures{
{"10000001",
{{BeginRepairsOperation{
.database = "someDb",
.collectionId = "10000001",
.collectionName = "follower10000001of10000002",
.protoCollectionId = "10000002",
.protoCollectionName = "prototype10000002",
.collectionReplicationFactor = 1,
.protoReplicationFactor = 1,
.renameDistributeShardsLike = true,
},
MoveShardOperation{
.database = "someDb",
.collectionId = "10000001",
.collectionName = "follower10000001of10000002",
.shard = "s11",
.from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
.to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
.isLeader = true,
},
FinishRepairsOperation{
.database = "someDb",
.collectionId = "10000001",
.collectionName = "follower10000001of10000002",
.protoCollectionId = "10000002",
.protoCollectionName = "prototype10000002",
.shards =
  {
    std::make_tuple<ShardID, ShardID, DBServers>(
      "s11", "s21",
      {"PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
  },
.replicationFactor = 1,
}}}},
{"10000003",
ResultT<std::vector<RepairOperation>>::error(
  TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS)
},
{"10000004",
ResultT<std::vector<RepairOperation>>::error(
  TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS)
},
{"10000005",
ResultT<std::vector<RepairOperation>>::error(
  TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)
},
{"10000006",
ResultT<std::vector<RepairOperation>>::error(
  TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)
},
{"10000007",
ResultT<std::vector<RepairOperation>>::error(
  TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)
},
{"10000099",
{{{BeginRepairsOperation{
.database = "someDb",
.collectionId = "10000099",
.collectionName = "follower10000099of10000098",
.protoCollectionId = "10000098",
.protoCollectionName = "prototype10000098",
.collectionReplicationFactor = 1,
.protoReplicationFactor = 1,
.renameDistributeShardsLike = true,
},
MoveShardOperation{
.database = "someDb",
.collectionId = "10000099",
.collectionName = "follower10000099of10000098",
.shard = "s991",
.from = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
.to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
.isLeader = true,
},
FinishRepairsOperation{
.database = "someDb",
.collectionId = "10000099",
.collectionName = "follower10000099of10000098",
.protoCollectionId = "10000098",
.protoCollectionName = "prototype10000098",
.shards =
  {
    std::make_tuple<ShardID, ShardID, DBServers>(
      "s991", "s981",
      {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"}),
  },
.replicationFactor = 1,
}}}}},
};
