/* jshint globalstrict:false, strict:false, unused: false */
/* global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, arango, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the replication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const errors = arangodb.errors;
const db = arangodb.db;

const replication = require('@arangodb/replication');
const compareTicks = require('@arangodb/replication-common').compareTicks;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const console = require('console');
const internal = require('internal');
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];
const mmfilesEngine = (db._engine().name === 'mmfiles');

const cn = 'UnitTestsReplication';
const cn2 = 'UnitTestsReplication2';
const systemCn = '_UnitTestsReplicationSys';

// these must match the values in the Makefile!
const replicatorUser = 'replicator-user';
const replicatorPassword = 'replicator-password';

const connectToMaster = function () {
  arango.reconnect(masterEndpoint, db._name(), replicatorUser, replicatorPassword);
};

const connectToSlave = function () {
  arango.reconnect(slaveEndpoint, db._name(), 'root', '');
};

const collectionChecksum = function (name) {
  var c = db._collection(name).checksum(true, true);
  return c.checksum;
};

const collectionCount = function (name) {
  return db._collection(name).count();
};

const compare = function (masterFunc, slaveFunc, applierConfiguration) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  connectToSlave();
  replication.applier.stop();

  while (replication.applier.state().state.running) {
    internal.wait(0.1, false);
  }

  var includeSystem = true;
  var restrictType = '';
  var restrictCollections = [];

  if (typeof applierConfiguration === 'object') {
    if (applierConfiguration.hasOwnProperty('includeSystem')) {
      includeSystem = applierConfiguration.includeSystem;
    }
    if (applierConfiguration.hasOwnProperty('restrictType')) {
      restrictType = applierConfiguration.restrictType;
    }
    if (applierConfiguration.hasOwnProperty('restrictCollections')) {
      restrictCollections = applierConfiguration.restrictCollections;
    }
  }

  var syncResult = replication.sync({
    endpoint: masterEndpoint,
    username: replicatorUser,
    password: replicatorPassword,
    verbose: true,
    includeSystem: includeSystem,
    restrictType: restrictType,
    restrictCollections: restrictCollections,
    waitForSyncTimeout: 120
  });

  db._flushCache();
  slaveFunc(state);

  assertTrue(syncResult.hasOwnProperty('lastLogTick'));

  applierConfiguration = applierConfiguration || {};
  applierConfiguration.endpoint = masterEndpoint;
  applierConfiguration.username = replicatorUser;
  applierConfiguration.password = replicatorPassword;
  applierConfiguration.includeSystem = includeSystem;

  if (!applierConfiguration.hasOwnProperty('chunkSize')) {
    applierConfiguration.chunkSize = 16384;
  }

  replication.applier.properties(applierConfiguration);
  replication.applier.start(syncResult.lastLogTick);

  var printed = false;

  while (true) {
    var slaveState = replication.applier.state();

    if (slaveState.state.lastError.errorNum > 0) {
      console.warn('slave has errored:', JSON.stringify(slaveState.state.lastError));
      break;
    }

    if (!slaveState.state.running) {
      console.warn('slave is not running');
      break;
    }

    if (compareTicks(slaveState.state.lastAppliedContinuousTick, syncResult.lastLogTick) >= 0 ||
        compareTicks(slaveState.state.lastProcessedContinuousTick, syncResult.lastLogTick) >= 0) {
      console.debug('slave has caught up. syncResult.lastLogTick:', syncResult.lastLogTick, 'slaveState.lastAppliedContinuousTick:', slaveState.state.lastAppliedContinuousTick, 'slaveState.lastProcessedContinuousTick:', slaveState.state.lastProcessedContinuousTick, 'slaveState:', slaveState);
      break;
    }

    if (!printed) {
      console.debug('waiting for slave to catch up');
      printed = true;
    }
    internal.wait(0.5, false);
  }

  db._flushCache();
  slaveFunc(state);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Base Test Config. Identitical part for _system and other DB
// //////////////////////////////////////////////////////////////////////////////

function BaseTestConfig () {
  'use strict';

  return {

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test invalid credentials
    // /////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials1: function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: replicatorUser,
        password: replicatorPassword + 'xx' // invalid
      };

      try {
        replication.applier.properties(configuration);
      } catch (err) {
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test invalid credentials
    // /////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials2: function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: replicatorUser + 'xx', // invalid
        password: replicatorPassword
      };

      try {
        replication.applier.properties(configuration);
      } catch (err) {
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test invalid credentials
    // /////////////////////////////////////////////////////////////////////////////

    testInvalidCredentials3: function () {
      var configuration = {
        endpoint: masterEndpoint,
        username: 'root',
        password: 'abc'
      };

      try {
        replication.applier.properties(configuration);
      } catch (err) {
        assertEqual(errors.ERROR_HTTP_UNAUTHORIZED.code, err.errorNum);
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test trx with multiple collections
    // /////////////////////////////////////////////////////////////////////////////

    testTrxMultiCollections: function () {
      connectToMaster();

      compare(
        function () {
          db._create(cn);
          db._create(cn2);

          db[cn].insert({
            _key: 'foo',
            value: 1
          });
          db[cn2].insert({
            _key: 'bar',
            value: 'A'
          });

          db._executeTransaction({
            collections: {
              write: [ cn, cn2 ]
            },
            action: function (params) {
              var c = require('internal').db._collection(params.cn);
              var c2 = require('internal').db._collection(params.cn2);

              c.replace('foo', {
                value: 2
              });
              c.insert({
                _key: 'foo2',
                value: 3
              });

              c2.replace('bar', {
                value: 'B'
              });
              c2.insert({
                _key: 'bar2',
                value: 'C'
              });
            },
            params: {
              cn,
              cn2
            }
          });
        },
        function () {
          assertEqual(2, db[cn].count());
          assertEqual(2, db[cn].document('foo').value);
          assertEqual(3, db[cn].document('foo2').value);

          assertEqual(2, db[cn2].count());
          assertEqual('B', db[cn2].document('bar').value);
          assertEqual('C', db[cn2].document('bar2').value);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test few documents
    // /////////////////////////////////////////////////////////////////////////////

    testFew: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 5000; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(5000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test many documents
    // /////////////////////////////////////////////////////////////////////////////

    testMany: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 50000; ++i) {
            docs.push({ value: i });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test many documents
    // /////////////////////////////////////////////////////////////////////////////

    testManyMore: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 150000; ++i) {
            docs.push({ value: i });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          state.checksum = collectionChecksum(cn);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(150000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test big markers
    // /////////////////////////////////////////////////////////////////////////////

    testBigMarkersObject: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);

          var doc = {};
          for (let i = 0; i < 1000; ++i) {
            doc['test' + i] = 'the quick brown foxx jumped over the LAZY dog';
          }

          for (let i = 0; i < 100; ++i) {
            c.save({
              'value': i,
              'values': doc
            });
          }

          var d = c.any();
          assertEqual(1000, Object.keys(d.values).length);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test big markers
    // /////////////////////////////////////////////////////////////////////////////

    testBigMarkersArray: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let doc = [];

          for (let i = 0; i < 1000; ++i) {
            doc.push('the quick brown foxx jumped over the LAZY dog');
          }

          for (let i = 0; i < 100; ++i) {
            c.save({
              'value': i,
              'values': doc
            });
          }

          var d = c.any();
          assertEqual(1000, d.values.length);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(100, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test heterogenous markers
    // /////////////////////////////////////////////////////////////////////////////

    testHeterogenousMarkers: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);
          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            let doc = {};
            doc['test' + i] = 'the quick brown foxx jumped over the LAZY dog';
            docs.push({
              'value': i,
              'values': doc
            });
          }
          c.insert(docs);

          var d = c.any();
          assertEqual(1, Object.keys(d.values).length);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test empty markers
    // /////////////////////////////////////////////////////////////////////////////

    testEmptyMarkers: function () {
      connectToMaster();

      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({});
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test documents
    // /////////////////////////////////////////////////////////////////////////////

    testDocuments1: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              'value': i,
              'foo': true,
              'bar': [i, false],
              'value2': null,
              'mydata': {
                'test': ['abc', 'def']
              }
            });
          }
          c.insert(docs);

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test documents
    // /////////////////////////////////////////////////////////////////////////////

    testDocuments2: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          for (let i = 0; i < 1000; ++i) {
            c.save({
              'abc': true,
              '_key': 'test' + i
            });
            if (i % 3 === 0) {
              c.remove('test' + i);
            } else if (i % 5 === 0) {
              c.update('test' + i, {
                'def': 'hifh'
              });
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(666, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test documents
    // /////////////////////////////////////////////////////////////////////////////

    testDocuments3: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 50000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'foo': 'bar',
              'baz': 'bat'
            });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test documents
    // /////////////////////////////////////////////////////////////////////////////

    testDocuments4: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 50000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'foo': 'bar',
              'baz': 'bat'
            });
            if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(50000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }, {
          chunkSize: 512
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test documents
    // /////////////////////////////////////////////////////////////////////////////

    testDocuments5: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          for (let i = 0; i < 100; ++i) {
            c.save({
              'abc': true,
              '_key': 'test' + i
            });
            if (i % 3 === 0) {
              c.remove('test' + i);
            } else if (i % 5 === 0) {
              c.update('test' + i, {
                'def': 'hifh'
              });
            }
          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(66, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test edges
    // /////////////////////////////////////////////////////////////////////////////

    testEdges: function () {
      compare(
        function (state) {
          let v = db._create(cn);
          let e = db._createEdgeCollection(cn2);

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i
            });
          }
          v.save(docs);

          docs = [];
          for (let i = 0; i < 5000; i += 10) {
            docs.push({
              _from: cn + '/test' + i,
              _to: cn + '/test' + i,
              'foo': 'bar',
              'value': i
            });
          }
          e.save(docs);

          state.checksum1 = collectionChecksum(cn);
          state.count1 = collectionCount(cn);
          assertEqual(1000, state.count1);

          state.checksum2 = collectionChecksum(cn2);
          state.count2 = collectionCount(cn2);
          assertEqual(500, state.count2);
        },
        function (state) {
          assertEqual(state.count1, collectionCount(cn));
          assertEqual(state.checksum1, collectionChecksum(cn));

          assertEqual(state.count2, collectionCount(cn2));
          assertEqual(state.checksum2, collectionChecksum(cn2));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionInsert: function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var c = require('internal').db._collection(params.cn);

              for (var i = 0; i < 1000; ++i) {
                c.save({
                  '_key': 'test' + i
                });
              }
            },
            params: { cn }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionInsertFail: function () {
      compare(
        function (state) {
          db._create(cn);

          try {
            db._executeTransaction({
              collections: {
                write: cn
              },
              action: function (params) {
                var c = require('@arangodb').db._collection(params.cn);
                for (var i = 0; i < 1000; ++i) {
                  c.save({
                    '_key': 'test' + i
                  });
                }

                throw new Error('rollback!');
              },
              params: { cn }
            });
            fail();
          } catch (err) {}

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionRemove: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i
            });
          }
          c.insert(docs);

          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var c = require('@arangodb').db._collection(params.cn);
              for (var i = 0; i < 1000; ++i) {
                c.remove('test' + i);
              }
            },
            params: { cn }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(0, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionRemoveFail: function () {
      compare(
        function (state) {
          let c = db._create(cn);

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i
            });
          }
          c.insert(docs);

          try {
            db._executeTransaction({
              collections: {
                write: cn
              },
              action: function (params) {
                var c = require('@arangodb').db._collection(params.cn);
                for (var i = 0; i < 1000; ++i) {
                  c.remove('test' + i);
                }
                throw new Error('peng!');
              },
              params: { cn }
            });
          } catch (err) {

          }

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionMultiOps: function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var c = require('internal').db._collection(params.cn);
              var i;

              for (i = 0; i < 1000; ++i) {
                c.save({
                  '_key': 'test' + i
                });
              }

              for (i = 0; i < 1000; ++i) {
                c.update('test' + i, {
                  'foo': 'bar' + i
                });
              }

              for (i = 0; i < 1000; ++i) {
                c.update('test' + i, {
                  'foo': 'baz' + i
                });
              }

              for (i = 0; i < 1000; i += 10) {
                c.remove('test' + i);
              }
            },
            params: {
              'cn': cn
            }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(900, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test big transaction
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionBig: function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var c = require('internal').db._collection(params.cn);
              var i;

              for (i = 0; i < 50000; ++i) {
                c.save({
                  '_key': 'test' + i,
                  value: i
                });
                c.update('test' + i, {
                  value: i + 1
                });

                if (i % 5 === 0) {
                  c.remove('test' + i);
                }
              }
            },
            params: {
              'cn': cn
            }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(40000, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }, {
          chunkSize: 2048
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test delayed transaction
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionDelayed: function () {
      compare(
        function (state) {
          db._create(cn);

          db._executeTransaction({
            collections: {
              write: cn
            },
            action: function (params) {
              var c = require('internal').db._collection(params.cn);
              var i;
              var wait = require('internal').wait;

              for (i = 0; i < 10; ++i) {
                c.save({
                  '_key': 'test' + i,
                  value: i
                });
                c.update('test' + i, {
                  value: i + 1
                });

                wait(1, false);
              }
            },
            params: {
              'cn': cn
            }
          });

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(10, state.count);
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));
        }, {
          chunkSize: 2048
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionMulti: function () {
      compare(
        function (state) {
          db._create(cn);
          db._create(cn2);

          db._executeTransaction({
            collections: {
              write: [cn, cn2]
            },
            action: function (params) {
              var c1 = require('internal').db._collection(params.cn);
              var c2 = require('internal').db._collection(params.cn2);
              var i;

              for (i = 0; i < 1000; ++i) {
                c1.save({
                  '_key': 'test' + i
                });
                c2.save({
                  '_key': 'test' + i,
                  'foo': 'bar'
                });
              }

              for (i = 0; i < 1000; ++i) {
                c1.update('test' + i, {
                  'foo': 'bar' + i
                });
              }

              for (i = 0; i < 1000; ++i) {
                c1.update('test' + i, {
                  'foo': 'baz' + i
                });
                c2.update('test' + i, {
                  'foo': 'baz' + i
                });
              }

              for (i = 0; i < 1000; i += 10) {
                c1.remove('test' + i);
                c2.remove('test' + i);
              }
            },
            params: {
              'cn': cn,
              'cn2': cn2
            }
          });

          state.checksum1 = collectionChecksum(cn);
          state.checksum2 = collectionChecksum(cn2);
          state.count1 = collectionCount(cn);
          state.count2 = collectionCount(cn2);
          assertEqual(900, state.count1);
          assertEqual(900, state.count2);
        },
        function (state) {
          assertEqual(state.count1, collectionCount(cn));
          assertEqual(state.count2, collectionCount(cn2));
          assertEqual(state.checksum1, collectionChecksum(cn));
          assertEqual(state.checksum2, collectionChecksum(cn2));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test transactions
    // /////////////////////////////////////////////////////////////////////////////

    testTransactionAbort: function () {
      compare(
        function (state) {
          db._create(cn);
          db._create(cn2);

          db._collection(cn).save({
            foo: 'bar'
          });
          db._collection(cn2).save({
            bar: 'baz'
          });

          try {
            db._executeTransaction({
              collections: {
                write: [cn, cn2]
              },
              action: function (params) {
                var c1 = require('internal').db._collection(params.cn);
                var c2 = require('internal').db._collection(params.cn2);
                var i;

                for (i = 0; i < 1000; ++i) {
                  c1.save({
                    '_key': 'test' + i
                  });
                  c2.save({
                    '_key': 'test' + i,
                    'foo': 'bar'
                  });
                }

                for (i = 0; i < 1000; ++i) {
                  c1.update('test' + i, {
                    'foo': 'bar' + i
                  });
                }

                for (i = 0; i < 1000; ++i) {
                  c1.update('test' + i, {
                    'foo': 'baz' + i
                  });
                  c2.update('test' + i, {
                    'foo': 'baz' + i
                  });
                }

                for (i = 0; i < 1000; i += 10) {
                  c1.remove('test' + i);
                  c2.remove('test' + i);
                }

                throw new Error('rollback!');
              },
              params: {
                'cn': cn,
                'cn2': cn2
              }
            });
            fail();
          } catch (err) {}

          state.checksum1 = collectionChecksum(cn);
          state.checksum2 = collectionChecksum(cn2);
          state.count1 = collectionCount(cn);
          state.count2 = collectionCount(cn2);
          assertEqual(1, state.count1);
          assertEqual(1, state.count2);
        },
        function (state) {
          assertEqual(state.count1, collectionCount(cn));
          assertEqual(state.count2, collectionCount(cn2));
          assertEqual(state.checksum1, collectionChecksum(cn));
          assertEqual(state.checksum2, collectionChecksum(cn2));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test rename collection
    // /////////////////////////////////////////////////////////////////////////////

    testRenameCollection1: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            isVolatile: mmfilesEngine,
            waitForSync: false,
            doCompact: false,
            journalSize: 1048576,
            keyOptions: {
              allowUserKeys: false
            }
          });

          c.rename(cn2);

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          try {
            db._collection(cn).properties();
            fail();
          } catch (err) {
            // original collection was renamed
          }

          var properties = db._collection(cn2).properties();
          assertEqual(cn2, db._collection(cn2).name());
          if (mmfilesEngine) {
            assertTrue(properties.isVolatile);
            assertFalse(properties.doCompact);
            assertEqual(1048576, properties.journalSize);
          }
          assertFalse(properties.waitForSync);
          assertFalse(properties.deleted);
          assertFalse(properties.keyOptions.allowUserKeys);
          assertEqual('traditional', properties.keyOptions.type);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test rename collection
    // /////////////////////////////////////////////////////////////////////////////

    testRenameCollection2: function () {
      compare(
        function (state) {
          var c = db._create(cn);
          c.rename(cn2);
          c.rename(cn);

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          try {
            db._collection(cn2).properties();
            fail();
          } catch (err) {
            // collection was renamed
          }

          assertEqual(cn, db._collection(cn).name());
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test change collection
    // /////////////////////////////////////////////////////////////////////////////

    testChangeCollection1: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            waitForSync: false,
            doCompact: false,
            journalSize: 1048576
          });

          var properties = c.properties();
          assertFalse(properties.waitForSync);
          if (mmfilesEngine) {
            assertFalse(properties.doCompact);
            assertEqual(1048576, properties.journalSize);
          }

          properties = c.properties({
            waitForSync: true,
            doCompact: true,
            journalSize: 2097152
          });
          assertTrue(properties.waitForSync);
          if (mmfilesEngine) {
            assertTrue(properties.doCompact);
            assertEqual(2097152, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
          }

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertTrue(properties.waitForSync);
          if (mmfilesEngine) {
            assertTrue(properties.doCompact);
            assertEqual(2097152, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
            assertEqual(properties.indexBuckets, properties.indexBuckets);
          }
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test change collection
    // /////////////////////////////////////////////////////////////////////////////

    testChangeCollection2: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            waitForSync: true,
            doCompact: true,
            journalSize: 2097152
          });

          var properties = c.properties();
          assertTrue(properties.waitForSync);
          if (mmfilesEngine) {
            assertTrue(properties.doCompact);
            assertEqual(2097152, properties.journalSize);
          }

          properties = c.properties({
            waitForSync: false,
            doCompact: false,
            journalSize: 1048576
          });
          assertFalse(properties.waitForSync);
          if (mmfilesEngine) {
            assertFalse(properties.doCompact);
            assertEqual(1048576, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
          }

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.waitForSync);
          if (mmfilesEngine) {
            assertFalse(properties.doCompact);
            assertEqual(1048576, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
            assertEqual(properties.indexBuckets, properties.indexBuckets);
          }
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test change collection
    // /////////////////////////////////////////////////////////////////////////////

    testChangeCollectionIndexBuckets: function () {
      if (mmfilesEngine) {
        compare(
          function (state) {
            var c = db._create(cn, {
              indexBuckets: 4
            });

            var properties = c.properties();
            assertEqual(4, properties.indexBuckets);

            properties = c.properties({
              indexBuckets: 8
            });
            assertEqual(8, properties.indexBuckets);

            state.cid = c._id;
            state.properties = c.properties();
          },
          function (state) {
            var properties = db._collection(cn).properties();
            assertEqual(8, properties.indexBuckets);
          }
        );
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test create collection
    // /////////////////////////////////////////////////////////////////////////////

    testCreateCollection1: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            isVolatile: mmfilesEngine,
            waitForSync: false,
            doCompact: false,
            journalSize: 1048576
          });

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.waitForSync);
          assertFalse(properties.deleted);
          if (mmfilesEngine) {
            assertTrue(properties.isVolatile);
            assertFalse(properties.doCompact);
            assertEqual(1048576, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
          }
          assertTrue(properties.keyOptions.allowUserKeys);
          assertEqual('traditional', properties.keyOptions.type);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test create collection
    // /////////////////////////////////////////////////////////////////////////////

    testCreateCollection2: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            keyOptions: {
              type: 'autoincrement',
              allowUserKeys: false
            },
            isVolatile: false,
            waitForSync: true,
            doCompact: true,
            journalSize: 2097152
          });

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.isVolatile);
          assertTrue(properties.waitForSync);
          assertFalse(properties.deleted);
          if (mmfilesEngine) {
            assertTrue(properties.doCompact);
            assertEqual(2097152, properties.journalSize);
            assertTrue(properties.hasOwnProperty('indexBuckets'));
          }
          assertFalse(properties.keyOptions.allowUserKeys);
          assertEqual('autoincrement', properties.keyOptions.type);
        }
      );
    },

    testCreateCollectionKeygenUuid: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            keyOptions: {
              type: 'uuid',
              allowUserKeys: false
            }
          });

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.keyOptions.allowUserKeys);
          assertEqual('uuid', properties.keyOptions.type);
        }
      );
    },

    testCreateCollectionKeygenPadded: function () {
      compare(
        function (state) {
          var c = db._create(cn, {
            keyOptions: {
              type: 'padded',
              allowUserKeys: false
            }
          });

          state.cid = c._id;
          state.properties = c.properties();
        },
        function (state) {
          var properties = db._collection(cn).properties();
          assertEqual(cn, db._collection(cn).name());
          assertFalse(properties.keyOptions.allowUserKeys);
          assertEqual('padded', properties.keyOptions.type);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test create collection
    // /////////////////////////////////////////////////////////////////////////////

    testCreateCollectionIndexBuckets1: function () {
      if (mmfilesEngine) {
        compare(
          function (state) {
            var c = db._create(cn, {
              indexBuckets: 16
            });

            state.cid = c._id;
            state.properties = c.properties();
          },
          function (state) {
            var properties = db._collection(cn).properties();
            assertEqual(cn, db._collection(cn).name());
            assertEqual(16, properties.indexBuckets);
          }
        );
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test create collection
    // /////////////////////////////////////////////////////////////////////////////

    testCreateCollectionIndexBuckets2: function () {
      if (mmfilesEngine) {
        compare(
          function (state) {
            var c = db._create(cn, {
              indexBuckets: 8
            });

            state.cid = c._id;
            state.properties = c.properties();
          },
          function (state) {
            var properties = db._collection(cn).properties();
            assertEqual(cn, db._collection(cn).name());
            assertEqual(8, properties.indexBuckets);
          }
        );
      }
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test drop collection
    // /////////////////////////////////////////////////////////////////////////////

    testDropCollection: function () {
      compare(
        function (state) {
          var c = db._create(cn);
          c.drop();
        },
        function (state) {
          assertNull(db._collection(cn));
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test hash index
    // /////////////////////////////////////////////////////////////////////////////

    testHashIndex: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureHashIndex('a', 'b');

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'a': parseInt(i / 2),
              'b': i
            });
          }
          c.insert(docs);
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1001, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('hash', state.idx.type);
          assertFalse(state.idx.unique);
          assertFalse(state.idx.sparse);
          assertEqual(['a', 'b'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test hash index
    // /////////////////////////////////////////////////////////////////////////////

    testSparseHashIndex: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureHashIndex('a', 'b', {
            sparse: true
          });

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'a': parseInt(i / 2),
              'b': i
            });
          }
          c.insert(docs);
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1001, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('hash', state.idx.type);
          assertFalse(state.idx.unique);
          assertTrue(state.idx.sparse);
          assertEqual(['a', 'b'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test unique constraint
    // /////////////////////////////////////////////////////////////////////////////

    testUniqueConstraint: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureUniqueConstraint('a');

          for (let i = 0; i < 1000; ++i) {
            try {
              c.save({
                '_key': 'test' + i,
                'a': parseInt(i / 2)
              });
            } catch (err) {}
          }
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(501, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('hash', state.idx.type);
          assertTrue(state.idx.unique);
          assertFalse(state.idx.sparse);
          assertEqual(['a'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test unique constraint
    // /////////////////////////////////////////////////////////////////////////////

    testSparseUniqueConstraint: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureUniqueConstraint('a', {
            sparse: true
          });

          for (let i = 0; i < 1000; ++i) {
            try {
              c.save({
                '_key': 'test' + i,
                'a': parseInt(i / 2)
              });
            } catch (err) {}
          }
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(501, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('hash', state.idx.type);
          assertTrue(state.idx.unique);
          assertTrue(state.idx.sparse);
          assertEqual(['a'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test skiplist
    // /////////////////////////////////////////////////////////////////////////////

    testSkiplist: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureSkiplist('a', 'b');

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'a': parseInt(i / 2),
              'b': i
            });
          }
          c.insert(docs);
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1001, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('skiplist', state.idx.type);
          assertFalse(state.idx.unique);
          assertFalse(state.idx.sparse);
          assertEqual(['a', 'b'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test skiplist
    // /////////////////////////////////////////////////////////////////////////////

    testSparseSkiplist: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureSkiplist('a', 'b', {
            sparse: true
          });

          let docs = [];
          for (let i = 0; i < 1000; ++i) {
            docs.push({
              '_key': 'test' + i,
              'a': parseInt(i / 2),
              'b': i
            });
          }
          c.insert(docs);
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(1001, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('skiplist', state.idx.type);
          assertFalse(state.idx.unique);
          assertTrue(state.idx.sparse);
          assertEqual(['a', 'b'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test unique skiplist
    // /////////////////////////////////////////////////////////////////////////////

    testUniqueSkiplist: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureUniqueSkiplist('a');

          for (let i = 0; i < 1000; ++i) {
            try {
              c.save({
                '_key': 'test' + i,
                'a': parseInt(i / 2)
              });
            } catch (err) {}
          }
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(501, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('skiplist', state.idx.type);
          assertTrue(state.idx.unique);
          assertFalse(state.idx.sparse);
          assertEqual(['a'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test unique skiplist
    // /////////////////////////////////////////////////////////////////////////////

    testUniqueSparseSkiplist: function () {
      compare(
        function (state) {
          let c = db._create(cn);
          c.ensureUniqueSkiplist('a', {
            sparse: true
          });

          for (let i = 0; i < 1000; ++i) {
            try {
              c.save({
                '_key': 'test' + i,
                'a': parseInt(i / 2)
              });
            } catch (err) {}
          }
          c.save({});

          state.checksum = collectionChecksum(cn);
          state.count = collectionCount(cn);
          assertEqual(501, state.count);

          state.idx = c.getIndexes()[1];
        },
        function (state) {
          assertEqual(state.count, collectionCount(cn));
          assertEqual(state.checksum, collectionChecksum(cn));

          var idx = db._collection(cn).getIndexes()[1];
          assertEqual(state.idx.id, idx.id);
          assertEqual('skiplist', state.idx.type);
          assertTrue(state.idx.unique);
          assertTrue(state.idx.sparse);
          assertEqual(['a'], state.idx.fields);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test system collection
    // /////////////////////////////////////////////////////////////////////////////

    testSystemCollectionWithDefaults: function () {
      compare(
        function (state) {
          var c = db._create(systemCn, {
            isSystem: true
          });
          c.save({
            _key: 'UnitTester',
            testValue: 42
          });
        },
        function (state) {
          var doc = db[systemCn].document('UnitTester');
          assertEqual(42, doc.testValue);
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test system collection
    // /////////////////////////////////////////////////////////////////////////////

    testSystemCollectionExcludeSystem: function () {
      compare(
        function (state) {
          var c = db._create(systemCn, {
            isSystem: true
          });
          c.save({
            _key: 'UnitTester',
            testValue: 42
          });
        },
        function (state) {
          assertNull(db._collection(systemCn));
        }, {
          includeSystem: false
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test system collection
    // /////////////////////////////////////////////////////////////////////////////

    testSystemCollectionExcludeCollection: function () {
      compare(
        function (state) {
          var c = db._create(systemCn, {
            isSystem: true
          });
          c.save({
            _key: 'UnitTester',
            testValue: 42
          });
        },
        function (state) {
          assertNull(db._collection(systemCn));
        }, {
          includeSystem: true,
          restrictType: 'exclude',
          restrictCollections: [systemCn]
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test system collection
    // /////////////////////////////////////////////////////////////////////////////

    testSystemCollectionIncludeCollection: function () {
      compare(
        function (state) {
          var c = db._create(systemCn, {
            isSystem: true
          });
          c.save({
            _key: 'UnitTester',
            testValue: 42
          });
        },
        function (state) {
          var doc = db[systemCn].document('UnitTester');
          assertEqual(42, doc.testValue);
        }, {
          includeSystem: true,
          restrictType: 'include',
          restrictCollections: [systemCn]
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test include/exclude collection
    // /////////////////////////////////////////////////////////////////////////////

    testCollectionIncludeCollection: function () {
      compare(
        function (state) {
          var c1 = db._create(cn);
          var c2 = db._create(cn2);
          c1.save({
            _key: 'UnitTester',
            testValue: 42
          });
          c2.save({
            _key: 'UnitTester',
            testValue: 23
          });
        },
        function (state) {
          var doc = db[cn].document('UnitTester');
          assertEqual(42, doc.testValue);
          assertNull(db._collection(cn2));
        }, {
          restrictType: 'include',
          restrictCollections: [cn]
        }
      );
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief test include/exclude collection
    // /////////////////////////////////////////////////////////////////////////////

    testCollectionExcludeCollection: function () {
      compare(
        function (state) {
          var c1 = db._create(cn);
          var c2 = db._create(cn2);
          c1.save({
            _key: 'UnitTester',
            testValue: 42
          });
          c2.save({
            _key: 'UnitTester',
            testValue: 23
          });
        },
        function (state) {
          var doc = db[cn].document('UnitTester');
          assertEqual(42, doc.testValue);
          assertNull(db._collection(cn2));
        }, {
          restrictType: 'exclude',
          restrictCollections: [cn2]
        }
      );
    },

    testViewBasic: function () {
      compare(
        function (state) {
          try {
            db._create(cn);
            let view = db._createView('UnitTestsSyncView', 'arangosearch', {});
            let links = {};
            links[cn] = {
              includeAllFields: true,
              fields: {
                text: { analyzers: [ 'text_en' ] }
              }
            };
            view.properties({'links': links});
            state.arangoSearchEnabled = true;
          } catch (err) { }
        },
        function (state) {
          if (!state.arangoSearchEnabled) {
            return;
          }

          let view = db._view('UnitTestsSyncView');
          assertTrue(view !== null);
          let props = view.properties();
          assertEqual(Object.keys(props.links).length, 1);
          assertTrue(props.hasOwnProperty('links'));
          assertTrue(props.links.hasOwnProperty(cn));
        }
      );
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite on _system
// //////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  'use strict';

  let suite = {

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief set up
    // /////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToSlave();
      try {
        replication.applier.stop();
        replication.applier.forget();
      } catch (err) { }
      connectToMaster();

      db._drop(cn);
      db._drop(cn2);
      db._drop(systemCn, { isSystem: true });
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief tear down
    // /////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      connectToMaster();

      db._drop(cn);
      db._drop(cn2);
      db._drop(systemCn, { isSystem: true });

      connectToSlave();
      replication.applier.stop();
      replication.applier.forget();
      db._drop(cn);
      db._drop(cn2);
      db._drop(systemCn, { isSystem: true });
    }
  };
  deriveTestSuite(BaseTestConfig(), suite, '_Repl');

  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite on other DB
// //////////////////////////////////////////////////////////////////////////////

function ReplicationOtherDBSuite () {
  const testDB = 'UnitTestDB';
  let suite = {
    // /////////////////////////////////////////////////////////////////////////////
    //  @brief set up
    // /////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._useDatabase('_system');
      // Slave side code
      connectToSlave();
      try {
        replication.applier.stop();
        replication.applier.forget();
      } catch (err) { }
      try {
        db._dropDatabase(testDB);
      } catch (e) {
      }
      db._createDatabase(testDB);
      assertNotEqual(-1, db._databases().indexOf(testDB));

      // Master side code
      connectToMaster();

      try {
        db._dropDatabase(testDB);
      } catch (e) {
      }
      db._createDatabase(testDB);

      assertNotEqual(-1, db._databases().indexOf(testDB));
      // We now have an empty testDB
      db._useDatabase(testDB);
      // Use it and setup replication
    },

    // /////////////////////////////////////////////////////////////////////////////
    //  @brief tear down
    // /////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      // Just drop the databases
      connectToMaster();
      db._useDatabase('_system');
      try {
        db._dropDatabase(testDB);
      } catch (e) {}

      assertEqual(-1, db._databases().indexOf(testDB));

      connectToSlave();
      // We need to stop applier in testDB
      db._useDatabase(testDB);
      replication.applier.stop();
      replication.applier.forget();
      db._useDatabase('_system');
      try {
        db._dropDatabase(testDB);
      } catch (e) {}

      assertEqual(-1, db._databases().indexOf(testDB));
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OtherRepl');

  return suite;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationSuite);
jsunity.run(ReplicationOtherDBSuite);

return jsunity.done();
