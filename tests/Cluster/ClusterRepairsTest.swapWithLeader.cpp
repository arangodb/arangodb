// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>>
  planCollections = R"=(
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
std::shared_ptr<VPackBuffer<uint8_t>>
  supervisionHealth3Healthy0Bad = R"=(
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

//std::shared_ptr<VPackBuffer<uint8_t>>
//  collName22222222vpack = R"=("22222222")="_vpack;
//Slice
//  collName22222222slice = Slice(collName22222222vpack->data());

std::map< CollectionID, std::vector< RepairOperation > >
  expectedOperationsWithTwoSwappedDBServers {
  {
    "11111111", {
// rename distributeShardsLike to repairingDistributeShardsLike
      BeginRepairsOperation {
        .database = "someDb",
        .collectionId = "11111111",
        .collectionName = "_frontend",
        .protoCollectionId = "22222222",
        .protoCollectionName = "_graphs",
        .collectionReplicationFactor = 2,
        .protoReplicationFactor = 2,
        .renameDistributeShardsLike = true,
      },
// shard s11 of collection 11111111
// make room on the dbserver where the leader should be
      MoveShardOperation {
        .database = "someDb",
        .collectionId = "11111111",
        .collectionName = "_frontend",
        .shard = "s11",
        .from = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
        .to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
        .isLeader = false,
      },
// move leader to the correct dbserver
      MoveShardOperation {
        .database = "someDb",
        .collectionId = "11111111",
        .collectionName = "_frontend",
        .shard = "s11",
        .from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
        .to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
        .isLeader = true,
      },
// fix the remaining shard
      MoveShardOperation {
        .database = "someDb",
        .collectionId = "11111111",
        .collectionName = "_frontend",
        .shard = "s11",
        .from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
        .to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
        .isLeader = false,
      },
// rename repairingDistributeShardsLike to distributeShardsLike
      FinishRepairsOperation {
        .database = "someDb",
        .collectionId = "11111111",
        .collectionName = "_frontend",
        .collectionShards = {},
        .protoCollectionId = "22222222",
        .protoCollectionName = "_graphs",
        .protoCollectionShards = {},
        .replicationFactor = 2,
      },
    }
  },
};
