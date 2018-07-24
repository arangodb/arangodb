// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "11111111": {
      "name": "leadingCollection",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
        ]
      },
      "replicationFactor": 4
    },
    "22222222": {
      "name": "followingCollection",
      "replicationFactor": 4,
      "distributeShardsLike": "11111111",
      "shards": {
        "s22": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth4Healthy0Bad = R"=(
{
  "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA": {
    "Status": "GOOD"
  },
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  },
  "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC": {
    "Status": "GOOD"
  },
  "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD": {
    "Status": "GOOD"
  }
}
)="_vpack;

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
    expectedResultsWithWronglyOrderedFollowers{
        {"22222222",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 _database = "someDb", _collectionId = "22222222",
                 _collectionName = "followingCollection",
                 _protoCollectionId = "11111111",
                 _protoCollectionName = "leadingCollection",
                 _collectionReplicationFactor = 4, _protoReplicationFactor = 4,
                 _renameDistributeShardsLike = true
             },
             // fix server order
             FixServerOrderOperation{
                 _database = "someDb", _collectionId = "22222222",
                 _collectionName = "followingCollection",
                 _protoCollectionId = "11111111",
                 _protoCollectionName = "leadingCollection", _shard = "s22",
                 _protoShard = "s11",
                 _leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 _followers =
                     {
                         "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
                     },
                 _protoFollowers =
                     {
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
                     }
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 _database = "someDb", _collectionId = "22222222",
                 _collectionName = "followingCollection",
                 _protoCollectionId = "11111111",
                 _protoCollectionName = "leadingCollection",
                 _shards =
                     {
                         std::make_tuple<ShardID, ShardID, DBServers>(
                             "s22", "s11",
                             {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                              "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                              "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                              "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"})
                     },
                 _replicationFactor = 4
             }
         }}}
    };
