/* global describe, it, before, beforeEach, afterEach */

// //////////////////////////////////////////////////////////////////////////////
// / @brief JavaScript cluster functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Andreas Streichardt
// //////////////////////////////////////////////////////////////////////////////

const wait = require('internal').wait;
const db = require('internal').db;
const cluster = require('@arangodb/cluster');
const expect = require('chai').expect;
const ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;

describe('Cluster sync', function() {
  before(function() {
    require('@arangodb/sync-replication-debug').setup();
  });

  describe('Databaseplan to local', function() {
    beforeEach(function() {
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
      });
    });
    it('should create a planned database', function() {
      let plan = {
        "Databases": {
          "test": {
            "id": 1,
            "name": "test"
          }
        }
      };
      let errors = cluster.executePlanForDatabases(plan.Databases);
      let databases = db._databases();
      expect(databases).to.have.lengthOf(2);
      expect(databases).to.contain('test');
      expect(errors).to.be.empty;
    });
    it('should leave everything in place if a planned database already exists', function() {
      let plan = {
        Databases: {
          "test": {
            "id": 1,
            "name": "test"
          }
        }
      };
      db._createDatabase('test');
      let errors = cluster.executePlanForDatabases(plan.Databases);
      let databases = db._databases();
      expect(databases).to.have.lengthOf(2);
      expect(databases).to.contain('test');
      expect(errors).to.be.empty;
    });
    it('should delete a database if it is not used anymore', function() {
      db._createDatabase('peng');
      let plan = {
        Databases: {
        }
      };
      cluster.executePlanForDatabases(plan.Databases);
      let databases = db._databases();
      expect(databases).to.have.lengthOf(1);
      expect(databases).to.contain('_system');
    });
  });
  describe('Collection plan to local', function() {
    let numSystemCollections;
    before(function() {
      require('@arangodb/sync-replication-debug').setup();
    });

    beforeEach(function() {
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
      });
      db._createDatabase('test');
      db._useDatabase('test');
      numSystemCollections = db._collections().length;
    });
    afterEach(function() {
      db._useDatabase('_system');
    });
    it('should create and load a collection if it does not exist', function() {
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "repltest",
                ]
              },
              "status": 3,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let collections = db._collections();
      expect(collections.map(collection => collection.name())).to.contain('s100001');
      expect(db._collection('s100001').status()).to.equal(ArangoCollection.STATUS_LOADED);
    });
    it('should create a collection if it does not exist (unloaded case)', function() {
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "repltest",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let collections = db._collections();
      expect(collections.map(collection => collection.name())).to.contain('s100001');
      let count = 0;
      while (db._collection('s100001').status() === ArangoCollection.STATUS_UNLOADING && count++ < 100) {
        wait(0.1);
      }
      expect(db._collection('s100001').status()).to.equal(ArangoCollection.STATUS_UNLOADED);
    });
    it('should unload an existing collection', function() {
      db._create('s100001');
      expect(db._collection('s100001').status()).to.equal(ArangoCollection.STATUS_LOADED);
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "test",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                  "repltest",
              ]
            },
            "status": 2,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      let count = 0;
      while (db._collection('s100001').status() === ArangoCollection.STATUS_UNLOADING && count++ < 100) {
        wait(0.1);
      }
      expect(db._collection('s100001').status()).to.equal(ArangoCollection.STATUS_UNLOADED);
    });
    it('should delete a stale collection', function() {
      db._create('s100001');
      let plan = {
        Collections: {
          test: {
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
    });
    it('should ignore a collection for which it is not responsible', function() {
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "swag",
                ]
              },
              "status": 3,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
    });
    it('should delete a collection for which it lost responsibility', function() {
      db._create('s100001');
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "debug-follower", // this is a different server than we are
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
    });
    it('should create an additional index if instructed to do so', function() {
      db._create('s100001');
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
                    "error": false,
                    "errorMessage": "",
                    "errorNum": 0,
                    "fields": [
                      "user"
                    ],
                    "id": "100005",
                    "sparse": true,
                    "type": "hash",
                    "unique": true
                  }
              ],
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "repltest",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let indexes = db._collection('s100001').getIndexes();
      expect(indexes).to.have.lengthOf(2);
    });
    it('should report failure when creating an index does not work', function() {
      let c = db._create('s100001');
      c.insert({"peng": "peng"});
      c.insert({"peng": "peng"});
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
                    "error": false,
                    "errorMessage": "",
                    "errorNum": 0,
                    "fields": [
                      "peng"
                    ],
                    "id": "100005",
                    "sparse": true,
                    "type": "hash",
                    "unique": true
                  }
              ],
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "repltest",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };

      let errors = cluster.executePlanForCollections(plan.Collections);
      expect(errors).to.have.property('s100001')
        .with.property('indexErrors')
        .with.property('100005');
    });
    it('should remove an additional index if instructed to do so', function() {
      db._create('s100001');
      db._collection('s100001').ensureIndex({ type: "hash", fields: [ "name" ] });
      let plan = {
        Databases: {
          "_system": {
            "id": 1,
            "name": "_system"
          },
          "test": {
            "id": 2,
            "name": "test"
          }
        },
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "s100001": [
                  "repltest",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan.Collections);
      db._useDatabase('test');
      let indexes = db._collection('s100001').getIndexes();
      expect(indexes).to.have.lengthOf(1);
    });
    it('should report an error when collection creation failed', function() {
      let plan = {
        Collections: {
          test: {
            "100001": {
              "deleted": false,
              "doCompact": true,
              "id": "100001",
              "indexBuckets": 8,
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
              "isSystem": false,
              "isVolatile": false,
              "journalSize": 1048576,
              "keyOptions": {
                "allowUserKeys": true,
                "type": "traditional"
              },
              "name": "test",
              "numberOfShards": 1,
              "replicationFactor": 2,
              "shardKeys": [
                "_key"
              ],
              "shards": {
                "Möter": [
                  "repltest",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      let errors = cluster.executePlanForCollections(plan.Collections);
      expect(errors).to.be.an('object');
      expect(errors).to.have.property('Möter');
    });
    it('should be leading a collection when ordered to be leader', function() {
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "test",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                  "repltest",
              ]
            },
            "status": 3,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      let errors = cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      expect(db._collection('s100001').getLeader()).to.equal("");
    });
    it('should be following a leader when ordered to be follower', function() {
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "test",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                "the leader-leader",
                  "repltest",
              ]
            },
            "status": 2,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      let errors = cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      expect(db._collection('s100001').getLeader()).to.equal("the leader-leader");
    });
    it('should be able to switch from leader to follower', function() {
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "test",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                  "repltest",
              ]
            },
            "status": 2,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      let errors = cluster.executePlanForCollections(plan);
      plan.test['100001'].shards['s100001'].unshift('der-hund');
      cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      expect(db._collection('s100001').getLeader()).to.equal("der-hund");
    });
    it('should be able to switch from follower to leader', function() {
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "test",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                "old-leader",
                  "repltest",
              ]
            },
            "status": 2,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      let errors = cluster.executePlanForCollections(plan);
      plan.test['100001'].shards['s100001'] = ["repltest"];
      cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      expect(db._collection('s100001').getLeader()).to.equal("");
    });
    it('should kill any unplanned server from current', function() {
      let collection = db._create('s100001');
      collection.setTheLeader("");
      collection.addFollower('test');
      collection.addFollower('test2');
      let plan = {
        test: {
          "100001": {
            "deleted": false,
            "doCompact": true,
            "id": "100001",
            "indexBuckets": 8,
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
            "isSystem": false,
            "isVolatile": false,
            "journalSize": 1048576,
            "keyOptions": {
              "allowUserKeys": true,
              "type": "traditional"
            },
            "name": "testi",
            "numberOfShards": 1,
            "replicationFactor": 2,
            "shardKeys": [
              "_key"
            ],
            "shards": {
              "s100001": [
                "repltest",
                "test2",
              ]
            },
            "status": 2,
            "type": 2,
            "waitForSync": false
          }
        }
      };
      cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      expect(collection.getFollowers()).to.deep.equal(['test2']);
    });
  });
  describe('Update current database', function() {
    beforeEach(function() {
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
      });
    });
    it('should report a new database', function() {
      db._createDatabase('testi');
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        }
      };
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(result).to.have.property('/arango/Current/Databases/testi/repltest');
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.property('op', 'set');
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.deep.property('new.name', 'testi');
    });
    it('should not do anything if there is nothing to do', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        }
      };
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(Object.keys(result)).to.have.lengthOf(0);
    });
    it('should remove a database from the agency if it is not present locally', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        },
        testi: {
          repltest: {
            id: 2,
            name: 'testi',
          }
        }
      };
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(result).to.have.property('/arango/Current/Databases/testi/repltest');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.property('op', 'delete');
    });
    it('should not delete other server database entries when removing', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        },
        testi: {
          repltest: {
            id: 2,
            name: 'testi',
          },
          testreplicant: {
            id: 2,
            name: 'testi',
          }
        }
      };
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(result).to.have.property('/arango/Current/Databases/testi/repltest');
      expect(Object.keys(result)).to.have.lengthOf(1);
    });
    it('should report any errors during database creation', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        },
      };
      let result = cluster.updateCurrentForDatabases({testi: {"error": true, name: "testi"}}, current);
      expect(result).to.have.property('/arango/Current/Databases/testi/repltest');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.property('op', 'set');
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.deep.property('new.error', true);
    });
    it('should override any previous error if creation finally worked', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        },
        testi: {
          repltest: {
            id: 2,
            name: 'testi',
            error: true,
          }
        },
      };
      db._createDatabase('testi');
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(result).to.have.property('/arango/Current/Databases/testi/repltest');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.property('op', 'set');
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.deep.property('new.name', 'testi');
      expect(result['/arango/Current/Databases/testi/repltest']).to.have.deep.property('new.error', false);
    });
    it('should not do anything if nothing happened', function() {
      let current = {
        _system: {
          repltest: {
            id: 1,
            name: '_system',
          },
        },
      };
      let result = cluster.updateCurrentForDatabases({}, current);
      expect(Object.keys(result)).to.have.lengthOf(0);
    });
  });
  describe('Update current collection', function() {
    beforeEach(function() {
      db._useDatabase('_system');
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
      });
      db._createDatabase('testung');
      db._useDatabase('testung');
    });
    it('should report a new collection in current', function() {
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      let current = {
      };
      let result = cluster.updateCurrentForCollections({}, current);
      expect(Object.keys(result)).to.have.length.of.at.least(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi');
      expect(result['/arango/Current/Collections/testung/888111/testi']).to.have.property('op', 'set');
      expect(result['/arango/Current/Collections/testung/888111/testi']).to.have.deep.property('new.servers')
        .that.is.an('array')
        .that.deep.equals(['repltest']);
    });
    it('should not do anything if nothing changed', function() {
      let current = {
      };
      let result = cluster.updateCurrentForCollections({}, current);
      expect(Object.keys(result)).to.have.lengthOf(0);
    });
    it('should not report any collections for which we are not leader (will be handled in replication)', function() {
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("NOBODY");  // mark collection as follower
      let current = {
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(Object.keys(result)).to.have.lengthOf(0);
    });
    it('should not delete any collections for which we are not a leader locally', function() {
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "the-master", "repltest" ] }
          },
        }
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(Object.keys(result)).to.have.lengthOf(0);
    });
    it('should resign leadership for which we are no more leader locally', function() {
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("NOBODY");
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "repltest" ] }
          },
        }
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi/servers')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi/servers')
        .that.has.property('new')
        .with.deep.equal(["_repltest"]);
    });
    it('should report newly assumed leadership for which we were a follower previously and remove any leaders and followers (these have to reregister themselves separately)', function() {
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "bogus-old-leader", "repltest", "useless-follower" ] }
          },
        }
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.servers')
        .with.deep.equal(["repltest"]);
    });
    it('should delete any collections for which we are not a leader locally', function() {
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "repltest" ] }
          },
        }
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('op', 'delete');
    });
    it('should report newly created indices', function() {
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      collection.ensureIndex({"type": "hash", "fields": ["hund"]});
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "repltest" ] }
          },
        }
      };
      let result = cluster.updateCurrentForCollections({}, {}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.indexes')
        .with.lengthOf(2);
    });
    it('should report collection errors', function() {
      let errors = {
        'testi': {
          collection: {
            'name': 'testi',
            'error': true,
            'errorNum': 666,
            'errorMessage': 'the number of the beast :S',
          },
          info: {
            database: 'testung',
            planId: '888111',
            shardName: 'testi',
          }
        }
      };
      let result = cluster.updateCurrentForCollections(errors, {});
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.error')
        .equals(true);
    });
    it('should report index errors', function() {
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false } ], "servers" : [ "repltest" ] }
          },
        }
      };

      let errors = {
        'testi': {
          'indexErrors': {
            1: {
              'id': 1,
              'error': true,
              'errorNum': 666,
              'errorMessage': 'the number of the beast :S',
            }
          },
        }
      };
      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      let result = cluster.updateCurrentForCollections(errors, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.indexes.1.error')
        .equals(true);
    });
    it('should report deleted indexes', function() {
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false },
                  {
                    "fields": [
                      "test"
                    ],
                    "id": "testi",
                    "selectivityEstimate": 1,
                    "sparse": false,
                    "type": "hash",
                    "unique": false
                  }
            ], "servers" : [ "repltest" ] }
          },
        }
      };

      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      let result = cluster.updateCurrentForCollections({}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.indexes')
        .that.has.lengthOf(1);
    });
    it('should report added indexes', function() {
      let current = {
        testung: {
          888111: {
            testi : { "error" : false, "errorMessage" : "", "errorNum" : 0, "indexes" : [ { "id" : "0", "type" : "primary", "fields" : [ "_key" ], "selectivityEstimate" : 1, "unique" : true, "sparse" : false },
            ], "servers" : [ "repltest" ] }
          },
        }
      };

      let props = { planId: '888111' };
      let collection = db._create('testi', props);
      collection.setTheLeader("");
      collection.ensureIndex({type: "hash", fields: ["test"]});
      let result = cluster.updateCurrentForCollections({}, current);
      expect(result).to.be.an('object');
      expect(Object.keys(result)).to.have.lengthOf(1);
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.have.property('op', 'set');
      expect(result).to.have.property('/arango/Current/Collections/testung/888111/testi')
        .that.has.deep.property('new.indexes')
        .that.has.lengthOf(2);
    });
  });
});
