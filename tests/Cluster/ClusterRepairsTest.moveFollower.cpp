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
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .protoCollectionId = "10000001",
                 .protoCollectionName = "prototype",
                 .collectionReplicationFactor = 3,
                 .protoReplicationFactor = 3,
                 .renameDistributeShardsLike = true,
             },
             // After a move, the new follower (here PRMR-C) will appear *last*
             // in the list, while the old (here PRMR-D) is removed. Thus the
             // order should be correct after this move, no FixServerOrder
             // should be needed for s21!
             MoveShardOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .shard = "s21",
                 .from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 .to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 .isLeader = false,
             },
             // No FixServerOrder should be necessary for s22, either.
             MoveShardOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .shard = "s22",
                 .from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 .to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 .isLeader = false,
             },
             // In contrast, for both s23 and s24 the order is wrong afterwards
             // and must be fixed!
             MoveShardOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .shard = "s23",
                 .from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 .to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 .isLeader = false,
             },
             FixServerOrderOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .protoCollectionId = "10000001",
                 .protoCollectionName = "prototype",
                 .shard = "s23",
                 .protoShard = "s13",
                 .leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 .followers =
                     {
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                     },
                 .protoFollowers =
                     {
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     },
             },
             MoveShardOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .shard = "s24",
                 .from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
                 .to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 .isLeader = false,
             },
             FixServerOrderOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .protoCollectionId = "10000001",
                 .protoCollectionName = "prototype",
                 .shard = "s24",
                 .protoShard = "s14",
                 .leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 .followers =
                     {
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                     },
                 .protoFollowers =
                     {
                         "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                         "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                     },
             },
             FinishRepairsOperation{
                 .database = "someDb",
                 .collectionId = "10000002",
                 .collectionName = "follower",
                 .protoCollectionId = "10000001",
                 .protoCollectionName = "prototype",
                 .shards =
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
                 .replicationFactor = 3,
             },
         }}},
    };
