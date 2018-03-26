// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>>
  planCollections = R"=(
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
std::shared_ptr<VPackBuffer<uint8_t>>
  supervisionHealth4Healthy0Bad = R"=(
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


std::map< CollectionID, std::vector< RepairOperation > >
  expectedOperationsWithWronglyOrderedFollowers {
  {
    "22222222", {
// rename distributeShardsLike to repairingDistributeShardsLike
      BeginRepairsOperation {
        .database = "someDb",
        .collectionId = "22222222",
        .collectionName = "followingCollection",
        .protoCollectionId = "11111111",
        .protoCollectionName = "leadingCollection",
        .collectionReplicationFactor = 4,
        .protoReplicationFactor = 4,
        .renameDistributeShardsLike = true,
      },
// fix server order
      FixServerOrderOperation {
        .database = "someDb",
        .collectionId = "22222222",
        .collectionName = "followingCollection",
        .protoCollectionId = "11111111",
        .protoCollectionName = "leadingCollection",
        .shard = "s22",
        .protoShard = "s11",
        .leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
        .followers = {
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
        },
        .protoFollowers = {
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
        },
      },
// rename repairingDistributeShardsLike to distributeShardsLike
      FinishRepairsOperation {
        .database = "someDb",
        .collectionId = "22222222",
        .collectionName = "followingCollection",
        .protoCollectionId = "11111111",
        .protoCollectionName = "leadingCollection",
        .shards = {
          std::make_tuple<ShardID, ShardID, DBServers>(
            "s22",
            "s11",
            {
              "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
              "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
              "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
              "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
            }
          ),
        },
        .replicationFactor = 4,
      },
    }
  },
};
