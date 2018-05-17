// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "10000001": {
      "name": "V",
      "isSmart": true,
      "replicationFactor": 2,
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000002": {
      "name": "E",
      "isSmart": true,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {}
    },
    "10000003": {
      "name": "_local_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s31": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000004": {
      "name": "_to_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s41": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000005": {
      "name": "_from_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s51": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth3Healthy0Bad = R"=(
{
  "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2": {
  },
  "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA": {
    "Status": "GOOD"
  },
  "CRDN-94ea8912-ff22-43d0-a005-bfc87f22709b": {
  },
  "CRDN-34b46cab-6f06-40a8-ac24-5eec1cf78f67": {
  },
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  },
  "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC": {
    "Status": "GOOD"
  }
}
)="_vpack;

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
    expectedResultsWithSmartGraph{
        {"10000003",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 _database = "someDb", _collectionId = "10000003",
                 _collectionName = "_local_E", _protoCollectionId = "10000001",
                 _protoCollectionName = "V", _collectionReplicationFactor = 2,
                 _protoReplicationFactor = 2,
                 _renameDistributeShardsLike = true
             },
             // shard s31 of collection 10000003
             // move follower
             MoveShardOperation{
                 _database = "someDb", _collectionId = "10000003",
                 _collectionName = "_local_E", _shard = "s31",
                 _from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 _isLeader = false
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 _database = "someDb", _collectionId = "10000003",
                 _collectionName = "_local_E", _protoCollectionId = "10000001",
                 _protoCollectionName = "V",
                 _shards =
                     {
                         std::make_tuple<ShardID, ShardID, DBServers>(
                             "s31", "s11",
                             {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                              "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"})
                     },
                 _replicationFactor = 2
             }
         }}},
        {"10000005",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 _database = "someDb", _collectionId = "10000005",
                 _collectionName = "_from_E", _protoCollectionId = "10000001",
                 _protoCollectionName = "V", _collectionReplicationFactor = 2,
                 _protoReplicationFactor = 2,
                 _renameDistributeShardsLike = true
             },
             // shard s51 of collection 10000005
             // move leader
             MoveShardOperation{
                 _database = "someDb", _collectionId = "10000005",
                 _collectionName = "_from_E", _shard = "s51",
                 _from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 _to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 _isLeader = true
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 _database = "someDb", _collectionId = "10000005",
                 _collectionName = "_from_E", _protoCollectionId = "10000001",
                 _protoCollectionName = "V",
                 _shards =
                     {
                         std::make_tuple<ShardID, ShardID, DBServers>(
                             "s51", "s11",
                             {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                              "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"})
                     },
                 _replicationFactor = 2
             }
         }}}
    };
