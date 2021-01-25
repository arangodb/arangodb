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
         {{// rename distributeShardsLike to repairingDistributeShardsLike
           BeginRepairsOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _protoCollectionId = "10000001",
               _protoCollectionName = "prototype",
               _collectionReplicationFactor = 3, _protoReplicationFactor = 3,
               _renameDistributeShardsLike = true
           },
           // After a move, the new follower (here PRMR-C) will appear *last*
           // in the list, while the old (here PRMR-D) is removed. Thus the
           // order should be correct after this move, no FixServerOrder
           // should be needed for s21!
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _shard = "s21",
               _from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
               _to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               _isLeader = false
           },
           // No FixServerOrder should be necessary for s22, either.
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _shard = "s22",
               _from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
               _to = "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
               _isLeader = false
           },
           // In contrast, for both s23 and s24 the order is wrong afterwards
           // and must be fixed!
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _shard = "s23",
               _from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
               _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
               _isLeader = false
           },
           FixServerOrderOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _protoCollectionId = "10000001",
               _protoCollectionName = "prototype", _shard = "s23",
               _protoShard = "s13",
               _leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
               _followers =
                   {
                       "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                       "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
                   },
               _protoFollowers =
                   {
                       "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                       "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
                   }
           },
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _shard = "s24",
               _from = "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
               _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
               _isLeader = false
           },
           FixServerOrderOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _protoCollectionId = "10000001",
               _protoCollectionName = "prototype", _shard = "s24",
               _protoShard = "s14",
               _leader = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
               _followers =
                   {
                       "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                       "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
                   },
               _protoFollowers =
                   {
                       "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                       "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
                   }
           },
           FinishRepairsOperation{
               _database = "someDb", _collectionId = "10000002",
               _collectionName = "follower", _protoCollectionId = "10000001",
               _protoCollectionName = "prototype",
               _shards =
                   {
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s21", "s11",
                           {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                            "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                            "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"}),
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s22", "s12",
                           {
                               "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                               "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                               "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
                           }),
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s23", "s13",
                           {
                               "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                               "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                               "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
                           }),
                       std::make_tuple<ShardID, ShardID, DBServers>(
                           "s24", "s14",
                           {
                               "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                               "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                               "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
                           })
                   },
               _replicationFactor = 3
           }}}}
    };
