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

std::shared_ptr<VPackBuffer<uint8_t>>
  collName11111111vpack = R"=("11111111")="_vpack;
Slice
  collName11111111slice = Slice(collName11111111vpack->data());

std::vector< RepairOperation >
  expectedOperationsWithRepairingDistributeShardsLike {
// rename repairingDistributeShardsLike to distributeShardsLike
  AgencyWriteTransaction {
    std::vector<AgencyOperation> {
      AgencyOperation {
        "Plan/Collections/someDb/22222222/repairingDistributeShardsLike",
        AgencySimpleOperationType::DELETE_OP,
      },
      AgencyOperation {
        "Plan/Collections/someDb/22222222/distributeShardsLike",
        AgencyValueOperationType::SET,
        collName11111111slice,
      },
    },
    std::vector<AgencyPrecondition> {
      AgencyPrecondition {
        "Plan/Collections/someDb/22222222/distributeShardsLike",
        AgencyPrecondition::Type::EMPTY,
        true,
      },
      AgencyPrecondition {
        "Plan/Collections/someDb/22222222/repairingDistributeShardsLike",
        AgencyPrecondition::Type::VALUE,
        collName11111111slice,
      },
    },
  },
};
