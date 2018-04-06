// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "10000001": {
      "name": "prototype",
      "replicationFactor": 3,
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s12": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s13": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s14": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000002": {
      "name": "follower",
      "replicationFactor": 3,
      "distributeShardsLike": "10000001",
      "shards": {
        "s21": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ],
        "s22": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
        ],
        "s23": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
        ],
        "s24": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth4Healthy0Bad = R"=(
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
  },
  "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD": {
    "Status": "GOOD"
  }
}
)="_vpack;

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
    expectedResultsWithFollowerOrder{
        {"10000002",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 "someDb", "10000002", "follower", "10000001", "prototype", 3,
                 3, true,
             },
             // After a move, the new follower (here PRMR-C) will appear *last*
             // in the list, while the old (here PRMR-D) is removed. Thus the
             // order should be correct after this move, no FixServerOrder
             // should be needed for s21!
             MoveShardOperation{
                 "someDb", "10000002", "follower", "s21",
                 "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC", false,
             },
             // No FixServerOrder should be necessary for s22, either.
             MoveShardOperation{
                 "someDb", "10000002", "follower", "s22",
                 "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC", false,
             },
             // In contrast, for both s23 and s24 the order is wrong afterwards
             // and must be fixed!
             MoveShardOperation{
                 "someDb", "10000002", "follower", "s23",
                 "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", false,
             },
             FixServerOrderOperation{
                 "someDb",
                 "10000002",
                 "follower",
                 "10000001",
                 "prototype",
                 "s23",
                 "s13",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 {
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 },
                 {
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 },
             },
             MoveShardOperation{
                 "someDb", "10000002", "follower", "s24",
                 "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", false,
             },
             FixServerOrderOperation{
                 "someDb",
                 "10000002",
                 "follower",
                 "10000001",
                 "prototype",
                 "s24",
                 "s14",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",

                 {
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 },

                 {
                     "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                     "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 },
             },
             FinishRepairsOperation{
                 "someDb",
                 "10000002",
                 "follower",
                 "10000001",
                 "prototype",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s21", "s11",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         }),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s22", "s12",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         }),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s23", "s13",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         }),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s24", "s14",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                             "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         }),
                 },
                 3,
             },
         }}},
    };
