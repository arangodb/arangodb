/* global describe, it, before, beforeEach */

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

const db = require('internal').db;
const cluster = require('@arangodb/cluster');
const expect = require('chai').expect;

describe('Cluster sync', function() {
  /*
  describe('Databaseplan to local', function() {
    before(function() {
      require('@arangodb/sync-replication-debug').setup();
    });

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
      let errors = cluster.executePlanForDatabases(plan);
      let databases = db._databases();
      console.error(databases);
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
      let errors = cluster.executePlanForDatabases(plan);
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
      cluster.executePlanForDatabases(plan);
      let databases = db._databases();
      expect(databases).to.have.lengthOf(1);
      expect(databases).to.contain('_system');
    });
  });
  describe('Collection plan to local', function() {
    let numSystemCollections;
    before(function() {
      require('@arangodb/sync-replication-debug').setup();
      numSystemCollections = db._collections().length;
    });

    beforeEach(function() {
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
        db._createDatabase('test');
      });
      db._useDatabase('test');
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
                  "",
                ]
              },
              "status": 3,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlanForCollections(plan);
      db._useDatabase('test');
      let collections = db._collections();
      console.error('was', collections.map(collection => collection.name()));
      expect(collections.map(collection => collection.name())).to.contain('s100001');
      expect(db._collection('test').status()).to.be(ArangoCollection.STATUS_LOADED);
    });
    it('should create a collection if it does not exist (unloaded case)', function() {
      let plan = {
        Databases: {
          "test": {
            "id": 1,
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
                  "repl-sync-test",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlan(plan);
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections + 1);
      expect(collections).to.contain('100001');
      expect(db._collection('test').status()).to.be(ArangoCollection.STATUS_LOADED);
    });
    it('should delete a stale collection', function() {
      db._createDatabase('test');
      db._create('s100001');
      let plan = {
        Databases: {
          "test": {
            "id": 1,
            "name": "test"
          }
        },
        Collections: {
          test: {
          }
        }
      };
      cluster.executePlan(plan);
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
    });
    it('should ignore a collection for which it is not responsible', function() {
      let plan = {
        Databases: {
          "test": {
            "id": 1,
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
                  "swag",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlan(plan);
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
      expect(collections).to.contain('100001');
      expect(db._collection('test').status()).to.be(ArangoCollection.STATUS_LOADED);
    });
    it('should delete a collection for which it lost responsibility', function() {
      db._createDatabase('test');
      db._create('s100001');
      let plan = {
        Databases: {
          "test": {
            "id": 1,
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
                  "swag", // this is a different server than we are
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlan(plan);
      let collections = db._collections();
      expect(collections).to.have.lengthOf(numSystemCollections);
      expect(collections).to.contain('100001');
      expect(db._collection('test').status()).to.be(ArangoCollection.STATUS_LOADED);
    });
    it('should create an additional index if instructed to do so', function() {
      db._createDatabase('test');
      db._create('s100001');
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
                  "repl-sync-test",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlan(plan);
      let indexes = db._collection('100001').getIndexes()
      expect(indexes).to.have.lengthOf(2);
    });
    it('should create an additional index if instructed to do so', function() {
      db._createDatabase('test');
      db._create('s100001');
      db._collection('s100001').ensureIndex({ type: "hash", fields: [ "name" ] })
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
                  "repl-sync-test",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      cluster.executePlan(plan);
      let indexes = db._collection('100001').getIndexes()
      expect(indexes).to.have.lengthOf(1);
    });
    it('should report an error when collection creation failed', function() {
      db._createDatabase('test');
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
            "Möter": {
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
                  "repl-sync-test",
                ]
              },
              "status": 2,
              "type": 2,
              "waitForSync": false
            }
          }
        }
      };
      let errors = cluster.executePlan(plan);
      expect(errors).to.be.an('object');
      expect(errors).to.have.property('Möter');
    });
  });
  describe('Update current', function() {
    beforeEach(function() {
      db._databases().forEach(database => {
        if (database !== '_system') {
          db._dropDatabase(database);
        }
      });
    });
    it('should report a new database', function() {
      let Current = {
        Databases: {},
      };
    });
  });
  */
});
