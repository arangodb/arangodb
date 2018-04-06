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
                 "someDb", "11111111", "_frontend", "22222222", "_graphs", 2, 2,
                 true,
             },
             // shard s11 of collection 11111111
             // make room on the dbserver where the leader should be
             MoveShardOperation{
                 "someDb", "11111111", "_frontend", "s11",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC", false,
             },
             // move leader to the correct dbserver
             MoveShardOperation{
                 "someDb", "11111111", "_frontend", "s11",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", true,
             },
             // fix the remaining shard
             MoveShardOperation{
                 "someDb", "11111111", "_frontend", "s11",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA", false,
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 "someDb",
                 "11111111",
                 "_frontend",
                 "22222222",
                 "_graphs",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s11", "s22",
                         {
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                         }),
                 },
                 2,
             },
         }}},
    };
