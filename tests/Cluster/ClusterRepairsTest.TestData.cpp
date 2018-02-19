// .[0].arango.Plan

std::shared_ptr<VPackBuilder> planBuilder = R"=(
{
  "DBServers": {
    "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a": "none",
    "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f": "none",
    "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9": "none"
  },
  "Collections": {
    "_system": {
    },
    "someDb": {
      "27050122": {
        "name": "_frontend",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050122",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050123": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050116": {
        "name": "_aqlfunctions",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050116",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050117": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050132": {
        "name": "_appbundles",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050132",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050133": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050125": {
        "name": "_queues",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050125",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050126": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050217": {
        "name": "_to_E",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050217",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 9,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "fields": [
              "_from"
            ],
            "id": "1",
            "sparse": false,
            "type": "edge",
            "unique": false
          },
          {
            "fields": [
              "_to"
            ],
            "id": "2",
            "sparse": false,
            "type": "edge",
            "unique": false
          }
        ],
        "statusString": "loaded",
        "type": 3,
        "shards": {
          "s27050237": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050239": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050238": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050236": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050243": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050240": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050244": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050242": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050241": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          ":_key"
        ],
        "distributeShardsLike": "27050204",
        "cacheEnabled": false
      },
      "27050204": {
        "name": "V",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050204",
        "isSystem": false,
        "isSmart": true,
        "numberOfShards": 9,
        "shardKeys": [
          "_key:"
        ],
        "smartGraphAttribute": "smartAttr",
        "statusString": "loaded",
        "type": 2,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "cacheEnabled": false,
        "waitForSync": false,
        "deleted": false,
        "replicationFactor": 2,
        "status": 3,
        "shards": {
          "s27050208": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050206": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050207": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050205": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050209": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050213": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050212": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050211": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050210": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        }
      },
      "27050215": {
        "name": "_local_E",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050215",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 9,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "fields": [
              "_from"
            ],
            "id": "1",
            "sparse": false,
            "type": "edge",
            "unique": false
          },
          {
            "fields": [
              "_to"
            ],
            "id": "2",
            "sparse": false,
            "type": "edge",
            "unique": false
          }
        ],
        "statusString": "loaded",
        "type": 3,
        "shards": {
          "s27050221": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050219": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050220": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050218": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050226": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050225": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050223": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050222": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050224": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key:"
        ],
        "distributeShardsLike": "27050204",
        "cacheEnabled": false
      },
      "27050216": {
        "name": "_from_E",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050216",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 9,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "fields": [
              "_from"
            ],
            "id": "1",
            "sparse": false,
            "type": "edge",
            "unique": false
          },
          {
            "fields": [
              "_to"
            ],
            "id": "2",
            "sparse": false,
            "type": "edge",
            "unique": false
          }
        ],
        "statusString": "loaded",
        "type": 3,
        "shards": {
          "s27050230": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050229": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050228": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050227": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ],
          "s27050235": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050232": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ],
          "s27050234": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050231": [
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f",
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
          ],
          "s27050233": [
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
            "PRMR-46cd11ba-ed50-423a-a087-5781e3cfbb0f"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key:"
        ],
        "distributeShardsLike": "27050204",
        "cacheEnabled": false
      },
      "27050214": {
        "name": "E",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050214",
        "isSystem": false,
        "isSmart": true,
        "numberOfShards": 0,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "fields": [
              "_from"
            ],
            "id": "1",
            "sparse": false,
            "type": "edge",
            "unique": false
          },
          {
            "fields": [
              "_to"
            ],
            "id": "2",
            "sparse": false,
            "type": "edge",
            "unique": false
          }
        ],
        "cacheEnabled": false,
        "shadowCollections": [
          27050215,
          27050216,
          27050217
        ],
        "statusString": "loaded",
        "type": 3,
        "status": 3,
        "shards": {},
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050204"
      },
      "27050111": {
        "name": "_routing",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050111",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050112": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050120": {
        "cacheEnabled": false,
        "deleted": false,
        "waitForSync": false,
        "distributeShardsLike": "27050107",
        "shardKeys": [
          "_key"
        ],
        "id": "27050120",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "statusString": "loaded",
        "type": 2,
        "isSystem": true,
        "shards": {
          "s27050121": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "name": "_apps",
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "deduplicate": true,
            "fields": [
              "mount"
            ],
            "id": "27050124",
            "sparse": false,
            "type": "hash",
            "unique": true
          }
        ],
        "numberOfShards": 1,
        "isSmart": false
      },
      "27050127": {
        "cacheEnabled": false,
        "deleted": false,
        "waitForSync": false,
        "distributeShardsLike": "27050107",
        "shardKeys": [
          "_key"
        ],
        "id": "27050127",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "statusString": "loaded",
        "type": 2,
        "isSystem": true,
        "shards": {
          "s27050128": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "name": "_jobs",
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          },
          {
            "deduplicate": true,
            "fields": [
              "queue",
              "status",
              "delayUntil"
            ],
            "id": "27050129",
            "sparse": false,
            "type": "skiplist",
            "unique": true
          },
          {
            "deduplicate": true,
            "fields": [
              "status",
              "queue",
              "delayUntil"
            ],
            "id": "27050130",
            "sparse": false,
            "type": "skiplist",
            "unique": true
          }
        ],
        "numberOfShards": 1,
        "isSmart": false
      },
      "27050109": {
        "name": "_modules",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050109",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "statusString": "loaded",
        "type": 2,
        "shards": {
          "s27050110": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        },
        "status": 3,
        "replicationFactor": 2,
        "deleted": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "distributeShardsLike": "27050107",
        "cacheEnabled": false
      },
      "27050107": {
        "name": "_graphs",
        "keyOptions": {
          "allowUserKeys": true,
          "lastValue": 0,
          "type": "traditional"
        },
        "id": "27050107",
        "isSystem": true,
        "isSmart": false,
        "numberOfShards": 1,
        "indexes": [
          {
            "fields": [
              "_key"
            ],
            "id": "0",
            "sparse": false,
            "type": "primary",
            "unique": true
          }
        ],
        "cacheEnabled": false,
        "shardKeys": [
          "_key"
        ],
        "statusString": "loaded",
        "type": 2,
        "waitForSync": false,
        "deleted": false,
        "replicationFactor": 2,
        "status": 3,
        "shards": {
          "s27050108": [
            "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
            "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
          ]
        }
      }
    }
  },
  "Coordinators": {
    "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2": "none",
    "CRDN-34b46cab-6f06-40a8-ac24-5eec1cf78f67": "none",
    "CRDN-94ea8912-ff22-43d0-a005-bfc87f22709b": "none"
  },
  "Databases": {
    "_system": {
      "id": "1",
      "name": "_system"
    },
    "someDb": {
      "coordinator": "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2",
      "id": "27050106",
      "name": "someDb",
      "options": {}
    }
  },
  "Version": 61,
  "Singles": {},
  "Lock": "UNLOCKED",
  "AsyncReplication": {}
}
)="_vpack;