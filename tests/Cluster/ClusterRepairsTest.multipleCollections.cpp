////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "10000001": {
      "name": "follower10000001of10000002",
      "replicationFactor": 2,
      "distributeShardsLike": "10000002",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000002": {
      "name": "prototype10000002",
      "replicationFactor": 2,
      "shards": {
        "s21": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000003": {
      "name": "follower10000003of10000002",
      "replicationFactor": 2,
      "distributeShardsLike": "10000002",
      "shards": {
        "s31": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000004": {
      "name": "prototype10000004",
      "replicationFactor": 1,
      "shards": {
        "s41": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000005": {
      "name": "follower10000005of10000004",
      "replicationFactor": 2,
      "distributeShardsLike": "10000004",
      "shards": {
        "s51": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000006": {
      "name": "prototype10000006",
      "replicationFactor": 1,
      "shards": {
        "s61": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000007": {
      "name": "follower10000007of10000006",
      "replicationFactor": 1,
      "distributeShardsLike": "10000006",
      "shards": {
        "s71": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000008": {
      "name": "prototype10000008",
      "replicationFactor": 4,
      "shards": {
        "s81": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
        ]
      }
    },
    "10000009": {
      "name": "follower10000009of10000008",
      "replicationFactor": 4,
      "distributeShardsLike": "10000008",
      "shards": {
        "s91": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
        ]
      }
    },
    "10000010": {
      "name": "prototype10000010",
      "replicationFactor": 3,
      "shards": {
        "s101": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000011": {
      "name": "follower10000011of10000010",
      "replicationFactor": 3,
      "distributeShardsLike": "10000010",
      "shards": {
        "s111": [
        ]
      }
    },
    "10000012": {
      "name": "prototype10000012",
      "replicationFactor": 1,
      "shards": {
        "s121": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ],
        "s122": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ],
        "s123": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000013": {
      "name": "follower10000013of10000012",
      "replicationFactor": 3,
      "distributeShardsLike": "10000012",
      "shards": {
        "s131": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ],
        "s132": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000098": {
      "name": "prototype10000098",
      "replicationFactor": 1,
      "shards": {
        "s981": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ]
      }
    },
    "10000099": {
      "name": "follower10000099of10000098",
      "replicationFactor": 1,
      "distributeShardsLike": "10000098",
      "shards": {
        "s991": [
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
        {"10000001",
         {{BeginRepairsOperation{
               _database = "someDb", _collectionId = "10000001",
               _collectionName = "follower10000001of10000002",
               _protoCollectionId = "10000002",
               _protoCollectionName = "prototype10000002",
               _collectionReplicationFactor = 2, _protoReplicationFactor = 2,
               _renameDistributeShardsLike = true
           },
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000001",
               _collectionName = "follower10000001of10000002", _shard = "s11",
               _from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
               _isLeader = false
           },
           FinishRepairsOperation{
               _database = "someDb", _collectionId = "10000001",
               _collectionName = "follower10000001of10000002",
               _protoCollectionId = "10000002",
               _protoCollectionName = "prototype10000002",
               _shards =
                   {
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s11", "s21",
                           {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                            "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"})
                   },
               _replicationFactor = 2
           }}}},
        {"10000003",
         {{BeginRepairsOperation{
               _database = "someDb", _collectionId = "10000003",
               _collectionName = "follower10000003of10000002",
               _protoCollectionId = "10000002",
               _protoCollectionName = "prototype10000002",
               _collectionReplicationFactor = 2, _protoReplicationFactor = 2,
               _renameDistributeShardsLike = true
           },
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000003",
               _collectionName = "follower10000003of10000002", _shard = "s31",
               _from = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               _to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
               _isLeader = true
           },
           FinishRepairsOperation{
               _database = "someDb", _collectionId = "10000003",
               _collectionName = "follower10000003of10000002",
               _protoCollectionId = "10000002",
               _protoCollectionName = "prototype10000002",
               _shards =
                   {
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s31", "s21",
                           {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                            "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"})
                   },
               _replicationFactor = 2
           }}}},
        {"10000005",
         ResultT<std::vector<RepairOperation>>::error(
             TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
             "replicationFactor is violated: Collection "
             "someDb/follower10000005of10000004 and its distributeShardsLike "
             "prototype someDb/prototype10000004 have 1 and 0 "
             "different (mismatching) DBServers, respectively.")},
        {"10000007",
         ResultT<std::vector<RepairOperation>>::error(
             TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
             "replicationFactor is violated: Collection "
             "someDb/follower10000007of10000006 and its distributeShardsLike "
             "prototype someDb/prototype10000006 have 1 and 0 different "
             "(mismatching) DBServers, respectively.")},
        {"10000009",  // replication factor too high
         ResultT<std::vector<RepairOperation>>::error(
             TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY)},
        {"10000011", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS)},
        {"10000013", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS)},
        {
            "10000099",
            {{{
                BeginRepairsOperation{
                    _database = "someDb", _collectionId = "10000099",
                    _collectionName = "follower10000099of10000098",
                    _protoCollectionId = "10000098",
                    _protoCollectionName = "prototype10000098",
                    _collectionReplicationFactor = 1,
                    _protoReplicationFactor = 1,
                    _renameDistributeShardsLike = true
                },
                MoveShardOperation{
                    _database = "someDb", _collectionId = "10000099",
                    _collectionName = "follower10000099of10000098",
                    _shard = "s991",
                    _from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                    _to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                    _isLeader = true
                },
                FinishRepairsOperation{
                    _database = "someDb", _collectionId = "10000099",
                    _collectionName = "follower10000099of10000098",
                    _protoCollectionId = "10000098",
                    _protoCollectionName = "prototype10000098",
                    _shards =
                        {
                            std::make_tuple<ShardID, ShardID, DBServers>(
                                "s991", "s981",
                                {"PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"})
                        },
                    _replicationFactor = 1
                }
            }}}
        }};
