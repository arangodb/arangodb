// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "11111111": {
      "name": "leadingCollection",
      "replicationFactor": 3,
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "22222222": {
      "name": "followingCollection",
      "replicationFactor": 3,
      "repairingDistributeShardsLike": "11111111",
      "shards": {
        "s22": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
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
    expectedResultsWithRepairingDistributeShardsLike{
        {"22222222",
         {{
             // begin without rename
             BeginRepairsOperation{
                 _database = "someDb", _collectionId = "22222222",
                 _collectionName = "followingCollection",
                 _protoCollectionId = "11111111",
                 _protoCollectionName = "leadingCollection",
                 _collectionReplicationFactor = 3, _protoReplicationFactor = 3,
                 _renameDistributeShardsLike = false
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
                              "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"})
                     },
                 _replicationFactor = 3
             }
         }}}};
