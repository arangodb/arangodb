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

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>>
  supervisionHealth2Healthy1Bad = R"=(
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
    "Status": "BAD"
  }
}
)="_vpack;

std::vector< std::shared_ptr< VPackBuffer<uint8_t> > >
  expectedTransactionsWithTwoSwappedDBServers {
// shard s11 of collection 11111111
// make room on the dbserver where the leader should be
R"=([
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "op": "set",
              "new": [
                "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
              ]
            }
          },
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "old": [
                "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
              ]
            }
          },
          "dummy-client-id"
        ])="_vpack,
// move leader to the correct dbserver
R"=([
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "op": "set",
              "new": [
                "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
              ]
            }
          },
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "old": [
                "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
              ]
            }
          },
          "dummy-client-id"
        ])="_vpack,
// fix the remaining shard
R"=([
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "op": "set",
              "new": [
                "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
              ]
            }
          },
          { "testArangoAgencyPrefix/Plan/Collections/someDb/11111111/shards/s11": {
              "old": [
                "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
              ]
            }
          },
          "dummy-client-id"
        ])="_vpack,
};
