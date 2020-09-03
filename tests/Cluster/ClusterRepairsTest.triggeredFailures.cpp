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
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s11": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000002": {
      "name": "prototype10000002",
      "replicationFactor": 1,
      "shards": {
        "s21": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    },
    "10000003": {
      "name": "follower10000003of10000002---fail_mismatching_leaders",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s31": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000004": {
      "name": "follower10000004of10000002---fail_mismatching_followers",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s41": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000005": {
      "name": "follower10000005of10000002---fail_inconsistent_attributes_in_repairDistributeShardsLike",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s51": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000006": {
      "name": "follower10000006of10000002---fail_inconsistent_attributes_in_createBeginRepairsOperation",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s61": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000007": {
      "name": "follower10000007of10000002---fail_inconsistent_attributes_in_createFinishRepairsOperation",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s71": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000008": {
      "name": "follower10000008of10000002---fail_no_dbservers",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s81": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000009": {
      "name": "follower10000009of10000002---fail_no_proto_dbservers",
      "replicationFactor": 1,
      "distributeShardsLike": "10000002",
      "shards": {
        "s91": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000098": {
      "name": "prototype10000098",
      "replicationFactor": 1,
      "shards": {
        "s981": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "10000099": {
      "name": "follower10000099of10000098",
      "replicationFactor": 1,
      "distributeShardsLike": "10000098",
      "shards": {
        "s991": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth2Healthy0Bad = R"=(
{
  "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2": {
  },
  "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA": {
    "Status": "GOOD"
  },
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  }
}
)="_vpack;

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
    expectedResultsWithTriggeredFailures{
        {"10000001",
         {{BeginRepairsOperation{
               _database = "someDb", _collectionId = "10000001",
               _collectionName = "follower10000001of10000002",
               _protoCollectionId = "10000002",
               _protoCollectionName = "prototype10000002",
               _collectionReplicationFactor = 1, _protoReplicationFactor = 1,
               _renameDistributeShardsLike = true
           },
           MoveShardOperation{
               _database = "someDb", _collectionId = "10000001",
               _collectionName = "follower10000001of10000002", _shard = "s11",
               _from = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
               _to = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
               _isLeader = true
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
                           {"PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"})
                   },
               _replicationFactor = 1
           }}}},
        {"10000003", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS)},
        {"10000004", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS)},
        {"10000005", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)},
        {"10000006", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)},
        {"10000007", ResultT<std::vector<RepairOperation>>::error(
                         TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES)},
        {"10000008", Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS)},
        {"10000009", Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS)},
        {"10000099",
         {{{BeginRepairsOperation{
                _database = "someDb", _collectionId = "10000099",
                _collectionName = "follower10000099of10000098",
                _protoCollectionId = "10000098",
                _protoCollectionName = "prototype10000098",
                _collectionReplicationFactor = 1, _protoReplicationFactor = 1,
                _renameDistributeShardsLike = true
            },
            MoveShardOperation{
                _database = "someDb", _collectionId = "10000099",
                _collectionName = "follower10000099of10000098", _shard = "s991",
                _from = "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                _to = "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
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
                            {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"})
                    },
                _replicationFactor = 1
            }}}}}
    };
