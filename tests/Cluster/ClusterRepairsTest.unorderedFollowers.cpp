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

std::shared_ptr<VPackBuffer<uint8_t>>
  collName11111111vpack = R"=("11111111")="_vpack;
Slice
  collName11111111slice = Slice(collName11111111vpack->data());

std::shared_ptr<VPackBuffer<uint8_t>>
  previousServerOrderVpack = R"=([
    "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
    "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD",
    "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
    "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
  ])="_vpack;
std::shared_ptr<VPackBuffer<uint8_t>>
  correctServerOrderVpack = R"=([
    "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
    "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
    "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
    "PRMR-DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD"
  ])="_vpack;
Slice previousServerOrderSlice = Slice(previousServerOrderVpack->data());
Slice correctServerOrderSlice = Slice(correctServerOrderVpack->data());

std::vector< RepairOperation >
  expectedOperationsWithWronglyOrderedFollowers {
// rename distributeShardsLike to repairingDistributeShardsLike
  AgencyWriteTransaction {
    std::vector<AgencyOperation> {
      AgencyOperation {
        "Plan/Collections/someDb/22222222/distributeShardsLike",
        AgencySimpleOperationType::DELETE_OP,
      },
      AgencyOperation {
        "Plan/Collections/someDb/22222222/repairingDistributeShardsLike",
        AgencyValueOperationType::SET,
        collName11111111slice,
      },
    },
    std::vector<AgencyPrecondition> {
      AgencyPrecondition {
        "Plan/Collections/someDb/22222222/distributeShardsLike",
        AgencyPrecondition::Type::VALUE,
        collName11111111slice,
      },
      AgencyPrecondition {
        "Plan/Collections/someDb/22222222/repairingDistributeShardsLike",
        AgencyPrecondition::Type::EMPTY,
        true,
      },
    },
  },
// fix server order
  AgencyWriteTransaction {
    AgencyOperation {
      "Plan/Collections/someDb/22222222/shards/s22",
      AgencyValueOperationType::SET,
      correctServerOrderSlice,
    },
    AgencyPrecondition {
      "Plan/Collections/someDb/22222222/shards/s22",
      AgencyPrecondition::Type::VALUE,
      previousServerOrderSlice,
    },
  },
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
