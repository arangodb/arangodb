// Agency output of .[0].arango.Plan.Collections
// TODO Use a collection with differing number of shard to its proto collection
// to induce an error
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "11111111": {
      "name": "follower11111111of22222222",
      "replicationFactor": 2,
      "distributeShardsLike": "22222222",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "22222222": {
      "name": "prototype22222222",
      "replicationFactor": 2,
      "shards": {
        "s21": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "33333333": {
      "name": "follower33333333of22222222",
      "replicationFactor": 2,
      "distributeShardsLike": "22222222",
      "shards": {
        "s31": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "44444444": {
      "name": "prototype44444444",
      "replicationFactor": 1,
      "shards": {
        "s41": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "55555555": {
      "name": "follower55555555of44444444",
      "replicationFactor": 2,
      "distributeShardsLike": "44444444",
      "shards": {
        "s51": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "66666666": {
      "name": "prototype66666666",
      "replicationFactor": 1,
      "shards": {
        "s61": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "77777777": {
      "name": "follower77777777of66666666",
      "replicationFactor": 1,
      "distributeShardsLike": "66666666",
      "shards": {
        "s71": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "88888888": {
      "name": "prototype88888888",
      "replicationFactor": 1,
      "shards": {
        "s81": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "99999999": {
      "name": "follower99999999of88888888",
      "replicationFactor": 1,
      "distributeShardsLike": "88888888",
      "shards": {
        "s91": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
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
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  },
  "CRDN-34b46cab-6f06-40a8-ac24-5eec1cf78f67": {
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
    expectedResultsWithMultipleCollections{
        {"11111111",
         {{BeginRepairsOperation{
               .database = "someDb",
               .collectionId = "11111111",
               .collectionName = "follower11111111of22222222",
               .protoCollectionId = "22222222",
               .protoCollectionName = "prototype22222222",
               .collectionReplicationFactor = 2,
               .protoReplicationFactor = 2,
               .renameDistributeShardsLike = true,
           },
           MoveShardOperation{
               .database = "someDb",
               .collectionId = "11111111",
               .collectionName = "follower11111111of22222222",
               .shard = "s11",
               .from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               .to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
               .isLeader = false,
           },
           FinishRepairsOperation{
               .database = "someDb",
               .collectionId = "11111111",
               .collectionName = "follower11111111of22222222",
               .protoCollectionId = "22222222",
               .protoCollectionName = "prototype22222222",
               .shards =
                   {
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s11", "s21",
                           {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                            "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
                   },
               .replicationFactor = 2,
           }}}},
        {"33333333",
         {{BeginRepairsOperation{
               .database = "someDb",
               .collectionId = "33333333",
               .collectionName = "follower33333333of22222222",
               .protoCollectionId = "22222222",
               .protoCollectionName = "prototype22222222",
               .collectionReplicationFactor = 2,
               .protoReplicationFactor = 2,
               .renameDistributeShardsLike = true,
           },
           MoveShardOperation{
               .database = "someDb",
               .collectionId = "33333333",
               .collectionName = "follower33333333of22222222",
               .shard = "s31",
               .from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               .to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
               .isLeader = true,
           },
           FinishRepairsOperation{
               .database = "someDb",
               .collectionId = "33333333",
               .collectionName = "follower33333333of22222222",
               .protoCollectionId = "22222222",
               .protoCollectionName = "prototype22222222",
               .shards =
                   {
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s31", "s21",
                           {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                            "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
                   },
               .replicationFactor = 2,
           }}}},
        {"55555555",
         ResultT<std::vector<RepairOperation>>::error(
             TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED)},
        {"77777777",
         ResultT<std::vector<RepairOperation>>::error(
             TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED)},
        {"99999999",
         {{{BeginRepairsOperation{
                .database = "someDb",
                .collectionId = "99999999",
                .collectionName = "follower99999999of88888888",
                .protoCollectionId = "88888888",
                .protoCollectionName = "prototype88888888",
                .collectionReplicationFactor = 1,
                .protoReplicationFactor = 1,
                .renameDistributeShardsLike = true,
            },
            MoveShardOperation{
                .database = "someDb",
                .collectionId = "99999999",
                .collectionName = "follower99999999of88888888",
                .shard = "s31",
                .from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                .to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                .isLeader = true,
            },
            FinishRepairsOperation{
                .database = "someDb",
                .collectionId = "99999999",
                .collectionName = "follower99999999of88888888",
                .protoCollectionId = "88888888",
                .protoCollectionName = "prototype88888888",
                .shards =
                    {
                        std::make_tuple<ShardID, ShardID, DBServers>(
                            "s31", "s21",
                            {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
                    },
                .replicationFactor = 1,
            }}}}},
    };
