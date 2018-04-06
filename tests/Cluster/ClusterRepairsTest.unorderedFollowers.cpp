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
                 "someDb", "22222222", "followingCollection", "11111111",
                 "leadingCollection", 4, 4, true,
             },
             // fix server order
             FixServerOrderOperation{
                 "someDb",
                 "22222222",
                 "followingCollection",
                 "11111111",
                 "leadingCollection",
                 "s22",
                 "s11",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 {
                     "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 },
                 {
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 },
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 "someDb",
                 "22222222",
                 "followingCollection",
                 "11111111",
                 "leadingCollection",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s22", "s11",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                             "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                         }),
                 },
                 4,
             },
         }}},
    };
