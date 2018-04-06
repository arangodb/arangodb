// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "10000001": {
      "name": "V",
      "isSmart": true,
      "replicationFactor": 2,
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000002": {
      "name": "E",
      "isSmart": true,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {}
    },
    "10000003": {
      "name": "_local_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s31": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000004": {
      "name": "_to_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s41": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000005": {
      "name": "_from_E",
      "isSmart": false,
      "replicationFactor": 2,
      "distributeShardsLike": "10000001",
      "shards": {
        "s51": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
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
    expectedResultsWithSmartGraph{
        {"10000003",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 "someDb", "10000003", "_local_E", "10000001", "V", 2, 2, true,
             },
             // shard s31 of collection 10000003
             // move follower
             MoveShardOperation{
                 "someDb", "10000003", "_local_E", "s31",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", false,
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 "someDb",
                 "10000003",
                 "_local_E",
                 "10000001",
                 "V",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s31", "s11",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                         }),
                 },
                 2,
             },
         }}},
        {"10000005",
         {{
             // rename distributeShardsLike to repairingDistributeShardsLike
             BeginRepairsOperation{
                 "someDb", "10000005", "_from_E", "10000001", "V", 2, 2, true,
             },
             // shard s51 of collection 10000005
             // move leader
             MoveShardOperation{
                 "someDb", "10000005", "_from_E", "s51",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA", true,
             },
             // rename repairingDistributeShardsLike to distributeShardsLike
             FinishRepairsOperation{
                 "someDb",
                 "10000005",
                 "_from_E",
                 "10000001",
                 "V",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s51", "s11",
                         {
                             "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                             "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                         }),
                 },
                 2,
             },
         }}},
    };
