// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "11111111": {
      "name": "_frontend",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      },
      "replicationFactor": 2,
      "distributeShardsLike": "22222222"
    },
    "22222222": {
      "name": "_graphs",
      "replicationFactor": 2,
      "shards": {
        "s22": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
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
    expectedResultsWithTwoSwappedDBServers{
        {"11111111",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 _database = "someDb", _collectionId = "11111111",
                 _collectionName = "_frontend", _protoCollectionId = "22222222",
                 _protoCollectionName = "_graphs",
                 _collectionReplicationFactor = 2, _protoReplicationFactor = 2,
                 _renameDistributeShardsLike = true
             },
             // shard s11 of collection 11111111
             // make room on the dbserver where the leader should be
             MoveShardOperation{
                 _database = "someDb", _collectionId = "11111111",
                 _collectionName = "_frontend", _shard = "s11",
                 _from = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 _to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 _isLeader = false
             },
             // move leader to the correct dbserver
             MoveShardOperation{
                 _database = "someDb", _collectionId = "11111111",
                 _collectionName = "_frontend", _shard = "s11",
                 _from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 _isLeader = true
             },
             // fix the remaining shard
             MoveShardOperation{
                 _database = "someDb", _collectionId = "11111111",
                 _collectionName = "_frontend", _shard = "s11",
                 _from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 _to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 _isLeader = false
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 _database = "someDb", _collectionId = "11111111",
                 _collectionName = "_frontend", _protoCollectionId = "22222222",
                 _protoCollectionName = "_graphs",
                 _shards =
                     {
                         std::make_tuple<ShardID, ShardID, DBServers>(
                             "s11", "s22",
                             {"PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                              "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"})
                     },
                 _replicationFactor = 2
             }
         }}}
    };
