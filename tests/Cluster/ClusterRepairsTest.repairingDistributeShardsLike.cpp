// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>>
  planCollections = R"=(
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

std::vector< RepairOperation >
  expectedOperationsWithRepairingDistributeShardsLike {
// begin without rename
  BeginRepairsOperation {
    .database = "someDb",
    .collectionId = "22222222",
    .collectionName = "followingCollection",
    .protoCollectionId = "11111111",
    .protoCollectionName = "leadingCollection",
    .collectionReplicationFactor = 3,
    .protoReplicationFactor = 3,
    .renameDistributeShardsLike = false,
  },
// rename repairingDistributeShardsLike to distributeShardsLike
  FinishRepairsOperation {
    .database = "someDb",
    .collectionId = "22222222",
    .collectionName = "followingCollection",
    .protoCollectionId = "11111111",
    .protoCollectionName = "leadingCollection",
    .replicationFactor = 3,
  },
};
