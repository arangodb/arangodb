/* global describe, it, arango, ARGUMENTS, after, before, beforeEach */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const arangodb = require("@arangodb");
const replication = require("@arangodb/replication");
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const compareTicks = replication.compareTicks;
const errors = arangodb.errors;
const db = arangodb.db;
const internal = require("internal");
const time = internal.time;

const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

const username = "root";
const password = "";

const dbName = "UnitTestDB";
const docColName = "UnitTestDocs";
const edgeColName = "UnitTestEdges";

const config = {
  endpoint: masterEndpoint,
  username: username,
  password: password,
  verbose: true,
  requireFromPresent: true
};

// We allow the replication a delay of this many seconds at most
const delay = 10;

// Flag if we need to reconnect.
let onMaster = true;

const compareIndexes = function(l, r, eq) {
  // This can modify l and r and remove id and selectivityEstimate
  expect(l).to.be.an("array");
  expect(r).to.be.an("array");
  for (let x of l) {
    delete x.id;
    delete x.selectivityEstimate;
  }
  for (let x of r) {
    delete x.id;
    delete x.selectivityEstimate;
  }
  if (eq) {
    expect(l).to.eql(r, JSON.stringify(l) + " vs. " + JSON.stringify(r));
  } else {
    expect(l).to.not.eql(r, JSON.stringify(l) + " vs. " + JSON.stringify(r));
  }
};


const waitForReplication = function() {
  const wasOnMaster = onMaster;
  connectToMaster();
  // use lastLogTick as of now
  const lastLogTick = replication.logger.state().state.lastUncommittedLogTick;
  // We only wait a defined time.
  const timeOut = time() + delay * 1000;
  connectToSlave();

  internal.sleep(0.5);  
  while (true) {
    // Guard to abort if failed to replicate
    expect(time()).to.be.below(timeOut, `Replication did not succeed for ${delay} seconds`);

    const state = replication.globalApplier.state().state;
    expect(state.lastError.errorNum).to.equal(0, `Error occured on slave: ${JSON.stringify(state.lastError)}`);
    expect(state.running).to.equal(true, "Slave is not running");
    
    if (compareTicks(state.lastAppliedContinuousTick, lastLogTick) >= 0 ||
        compareTicks(state.lastProcessedContinuousTick, lastLogTick) >= 0) {
      // Replication caught up.
      break;
    }
    internal.sleep(1.0);
  }
  //internal.print(state);
  //internal.print("lastLogTick: " + lastLogTick);

  if (wasOnMaster) {
    connectToMaster();
  } else {
    internal.wait(1.0, false);
    connectToSlave();
  }
};

// We always connect to _system DB because it is always present.
// You do not need to monitor any state.
const connectToMaster = function() {
  if (!onMaster) {
    reconnectRetry(masterEndpoint, "_system", username, password);
    db._flushCache();
    onMaster = true;
  } else {
    db._useDatabase("_system");
  }
};

const connectToSlave = function() {
  if (onMaster) {
    reconnectRetry(slaveEndpoint, "_system", username, password);
    db._flushCache();
    onMaster = false;
  } else {
    db._useDatabase("_system");
  }
};

const testCollectionExists = function(name) {
  expect(db._collections().map(function(c) { return c.name(); }).indexOf(name)).to.not.equal(-1, 
    `Collection ${name} does not exist although it should`);
};

const testCollectionDoesNotExists = function(name) {
  expect(db._collections().map(function(c) { return c.name(); }).indexOf(name)).to.equal(-1, 
    `Collection ${name} does exist although it should not`);
};

const testDBDoesExist= function(name) {
  expect(db._databases().indexOf(name)).to.not.equal(-1, 
    `Database ${name} does not exist although it should`);
};

const testDBDoesNotExist = function(name) {
  expect(db._databases().indexOf(name)).to.equal(-1, 
    `Database ${name} does exist although it should not`);
};

const cleanUpAllData = function () {
  // Drop Created collections
  try {
    db._dropDatabase(dbName);
  } catch (e) {
    // We ignore every error here, correctness needs to be validated during tests.
  }
  try {
    db._drop(docColName);
  } catch (e) {
    // We ignore every error here, correctness needs to be validated during tests.
  }
  try {
    db._drop(edgeColName);
  } catch (e) {
    // We ignore every error here, correctness needs to be validated during tests.
  }

  // Validate everthing is as expected.
  testDBDoesNotExist(dbName);
  testCollectionDoesNotExists(docColName);
  testCollectionDoesNotExists(edgeColName);
};

const startReplication = function() {
  // Setup global replication
  connectToSlave();

  replication.setupReplicationGlobal(config);
  waitForReplication();
};

const stopReplication = function () {
  // Clear the slave
  connectToSlave(); 

  // First stop replication
  try {
    replication.globalApplier.stop();
  } catch (e) {
    // We ignore every error here, correctness needs to be validated during tests.
  }
  try {
    replication.globalApplier.forget();
  } catch (e) {
    // We ignore every error here, correctness needs to be validated during tests.
  }

  while (replication.globalApplier.state().state.running) {
    internal.sleep(0.1);
  }
};

const cleanUp = function() {
  stopReplication();
  cleanUpAllData();

  connectToMaster();
  cleanUpAllData();
};

describe('Global Replication on a fresh boot', function () {

  before(function() {
    cleanUp();

    // Setup global replication
    startReplication();
  });

  after(cleanUp);


  describe("In _system database", function () {

    before(function() {
      db._useDatabase("_system");
    });

    it("should create and drop an empty document collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._create(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      // Validate it is created properly
      waitForReplication();

      connectToSlave();
      testCollectionExists(docColName);
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);

      connectToMaster();
      // Second Part Drop it again
      db._drop(docColName);
      testCollectionDoesNotExists(docColName);

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionDoesNotExists(docColName);
    });
    
    it("should create and drop an empty edge collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._createEdgeCollection(edgeColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionExists(edgeColName);
      let scol = db._collection(edgeColName);
      expect(scol.type()).to.equal(3);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);

      connectToMaster();
      // Second Part Drop it again
      db._drop(edgeColName);
      testCollectionDoesNotExists(edgeColName);

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionDoesNotExists(edgeColName);
    });

    it("should replicate database creation and dropping", function () {
      // First validate that there are no leftovers.
      connectToSlave();
      testDBDoesNotExist(dbName);

      connectToMaster();

      testDBDoesNotExist(dbName);

      // Now create database
      db._createDatabase(dbName);
      testDBDoesExist(dbName);

      waitForReplication();
      connectToSlave();

      // Validate it exists.
      testDBDoesExist(dbName);

      // Now delete it again.
      connectToMaster();
      db._dropDatabase(dbName);
      testDBDoesNotExist(dbName);

      waitForReplication();
      connectToSlave();
      testDBDoesNotExist(dbName);
    });

    describe("modify an existing collection", function () {

      before(function() {
        connectToMaster();
        db._create(docColName);

        connectToSlave();
        // Validate it is created properly
        waitForReplication();
        testCollectionExists(docColName);
      });

      after(function() {
        connectToMaster();
        db._drop(docColName);

        connectToSlave();
        // Validate it is created properly
        waitForReplication();
        testCollectionDoesNotExists(docColName);
      });

      it("should replicate documents", function () {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({value: i});
        }

        connectToMaster();
        db._collection(docColName).save(docs);

        waitForReplication();

        connectToSlave();
        expect(db._collection(docColName).count()).to.equal(100);
      });

      it("should replicate a complete document", function () {
        const key = "MyTestCreateDocument123";
        let doc = {
          _key: key,
          value: 123,
          foo: "bar"
        };

        connectToMaster();
        db._collection(docColName).save(doc);

        let original = db._collection(docColName).document(key);

        waitForReplication();

        connectToSlave();
        let replica = db._collection(docColName).document(key);
        expect(replica).to.deep.equal(original);
      });

      it("should replicate an update to a document", function () {
        const key = "MyTestUpdateDocument";

        let doc = {
          _key: key,
          value: 123
        };

        connectToMaster();
        db._collection(docColName).save(doc);

        waitForReplication();

        connectToMaster();
        db._collection(docColName).update(key, {foo: "bar"});

        let original = db._collection(docColName).document(key);

        connectToSlave();
        waitForReplication();

        let replica = db._collection(docColName).document(key);
        expect(replica).to.deep.equal(original);
      });

      it("should replicate a document removal", function () {
        const key = "MyTestRemoveDocument";

        let doc = {
          _key: key,
          value: 123
        };

        connectToMaster();
        db._collection(docColName).save(doc);

        waitForReplication();

        connectToMaster();
        db._collection(docColName).remove(key);

        waitForReplication();

        connectToSlave();
        expect(db._collection(docColName).exists(key)).to.equal(false);
      });

      it("should replicate index creation", function () {
        connectToMaster();

        let oIdx = db._collection(docColName).getIndexes();

        db._collection(docColName).ensureHashIndex("value");

        let mIdx = db._collection(docColName).getIndexes();

        waitForReplication();
        connectToSlave();

        internal.sleep(5); // makes test more reliable
        let sIdx = db._collection(docColName).getIndexes();
        compareIndexes(sIdx, mIdx, true);
        compareIndexes(sIdx, oIdx, false);
      });

      it("should replicate index creation with data", function () {
        connectToMaster();

        let c = db._collection(docColName);
        let oIdx = c.getIndexes();

        c.truncate({ compact: false });
        let docs = [];
        for(let i = 1; i <= 10000; i++) {
          docs.push({value2 : i});
          if (i % 1000 === 0) {
            c.save(docs);
            docs = [];
          }
        }

        c.ensureHashIndex("value2");
        let mIdx = c.getIndexes();

        waitForReplication();
        connectToSlave();

        internal.sleep(5); // makes test more reliable
        let sIdx = db._collection(docColName).getIndexes();
        expect(db._collection(docColName).count()).to.eq(10000);

        compareIndexes(sIdx, mIdx, true);
        compareIndexes(sIdx, oIdx, false);
      });
    });
  });

  describe(`In ${dbName} database`, function () {

    before(function() {
      connectToMaster();
      testDBDoesNotExist(dbName);

      db._createDatabase(dbName);
      testDBDoesExist(dbName);

      waitForReplication();

      connectToSlave();
      testDBDoesExist(dbName);
    });

    after(function() {
      connectToSlave();
      testDBDoesExist(dbName);

      connectToMaster();
      // Flip always uses _system
      testDBDoesExist(dbName);
      db._dropDatabase(dbName);
      testDBDoesNotExist(dbName);

      waitForReplication();
      connectToSlave();
      testDBDoesNotExist(dbName); 
    });

    it("should create and drop an empty document collection", function () {
      connectToMaster();
      db._useDatabase(dbName);
      // First Part Create Collection
      let mcol = db._create(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();
      testCollectionExists(docColName); 

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      db._useDatabase(dbName);
      testCollectionExists(docColName); 
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);

      connectToMaster();
      db._useDatabase(dbName);
      // Second Part Drop it again
      db._drop(docColName);
      testCollectionDoesNotExists(docColName);

      connectToSlave();
      db._useDatabase(dbName);
      // Validate it is created properly
      waitForReplication();
      testCollectionDoesNotExists(docColName);
    });

    it("should create and drop an empty edge collection", function () {
      connectToMaster();
      db._useDatabase(dbName);
      // First Part Create Collection
      let mcol = db._createEdgeCollection(edgeColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      db._useDatabase(dbName);
      testCollectionExists(edgeColName);
      let scol = db._collection(edgeColName);
      expect(scol.type()).to.equal(3);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);

      connectToMaster();
      db._useDatabase(dbName);
      // Second Part Drop it again
      db._drop(edgeColName);
      testCollectionDoesNotExists(edgeColName);

      connectToSlave();
      // Validate it is deleted properly
      waitForReplication();
      db._useDatabase(dbName);
      testCollectionDoesNotExists(edgeColName);
    });

    describe("modify an existing collection", function () {

      before(function() {
        connectToMaster();
        db._useDatabase(dbName);
        db._create(docColName);

        connectToSlave();
        // Validate it is created properly
        waitForReplication();
        db._useDatabase(dbName);
        testCollectionExists(docColName);
      });

      after(function() {
        connectToMaster();
        db._useDatabase(dbName);
        db._drop(docColName);

        connectToSlave();
        // Validate it is created properly
        waitForReplication();
        db._useDatabase(dbName);
        testCollectionDoesNotExists(docColName);
      });

      it("should replicate documents", function () {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({value: i});
        }

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).save(docs);

        waitForReplication();

        connectToSlave();
        db._useDatabase(dbName);
        expect(db._collection(docColName).count()).to.equal(100);
      });

      it("should replicate a complete document", function () {
        const key = "MyTestCreateDocument123";
        let doc = {
          _key: key,
          value: 123,
          foo: "bar"
        };

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).save(doc);

        let original = db._collection(docColName).document(key);

        waitForReplication();

        connectToSlave();
        db._useDatabase(dbName);
        let replica = db._collection(docColName).document(key);
        expect(replica).to.deep.equal(original);
      });

      it("should replicate an update to a document", function () {
        const key = "MyTestUpdateDocument";

        let doc = {
          _key: key,
          value: 123
        };

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).save(doc);

        waitForReplication();

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).update(key, {foo: "bar"});

        let original = db._collection(docColName).document(key);

        connectToSlave();
        waitForReplication();

        db._useDatabase(dbName);
        let replica = db._collection(docColName).document(key);
        expect(replica).to.deep.equal(original);
      });

      it("should replicate a document removal", function () {
        const key = "MyTestRemoveDocument";

        let doc = {
          _key: key,
          value: 123
        };

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).save(doc);

        waitForReplication();

        connectToMaster();
        db._useDatabase(dbName);
        db._collection(docColName).remove(key);

        waitForReplication();

        connectToSlave();
        db._useDatabase(dbName);
        expect(db._collection(docColName).exists(key)).to.equal(false);
      });

      it("should replicate index creation", function () {
        connectToMaster();
        db._useDatabase(dbName);
        let oIdx = db._collection(docColName).getIndexes();

        db._collection(docColName).ensureHashIndex("value");

        let mIdx = db._collection(docColName).getIndexes();

        waitForReplication();
        connectToSlave();
        db._useDatabase(dbName);

        internal.sleep(5); // makes test more reliable
        let sIdx = db._collection(docColName).getIndexes();
        
        compareIndexes(sIdx, mIdx, true);
        compareIndexes(sIdx, oIdx, false);
      });

      it("should replicate index creation with data", function () {
        connectToMaster();
        db._useDatabase(dbName);

        let c = db._collection(docColName);
        let oIdx = c.getIndexes();

        c.truncate();
        let docs = [];
        for(let i = 1; i <= 10000; i++) {
          docs.push({value2 : i});
          if (i % 1000 === 0) {
            c.save(docs);
            docs = [];
          }
        }

        c.ensureHashIndex("value2");
        let mIdx = c.getIndexes();

        waitForReplication();
        connectToSlave();
        db._useDatabase(dbName);

        internal.sleep(5); // makes test more reliable
        let sIdx = db._collection(docColName).getIndexes();
        expect(db._collection(docColName).count()).to.eq(10000);

        compareIndexes(sIdx, mIdx, true);
        compareIndexes(sIdx, oIdx, false);
      });
    });

  });
});

const fillMasterWithInitialData = function () {
  connectToMaster();
  testDBDoesNotExist(dbName);
  testCollectionDoesNotExists(docColName);
  testCollectionDoesNotExists(edgeColName);

  let docs = [];
  for (let i = 0; i < 100; ++i) {
    docs.push({value: i});
  }
  let col = db._create(docColName);
  col.ensureHashIndex("value");
  db._createEdgeCollection(edgeColName);

  col.save(docs);

  db._createDatabase(dbName);

  db._useDatabase(dbName);

  let dcol = db._create(docColName);
  dcol.ensureHashIndex("value");
  db._createEdgeCollection(edgeColName);

  dcol.save(docs);
  db._useDatabase("_system");
};

describe('Setup global replication on empty slave and master has some data', function () {
  
  before(function() {
    cleanUp();

    fillMasterWithInitialData();

    connectToSlave();

    // Validate slave is empty!
    testDBDoesNotExist(dbName);
    testCollectionDoesNotExists(docColName);
    testCollectionDoesNotExists(edgeColName);

    startReplication();
  });

  after(cleanUp);


  describe("In _system database", function () {

    before(function() {
      db._useDatabase("_system");
    });
    
    it("should have synced the document collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._collection(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      testCollectionExists(docColName);
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);
    });

    it("should have synced the edge collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._collection(edgeColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();

      // Validate it is created properly
      testCollectionExists(edgeColName);
      let scol = db._collection(edgeColName);
      expect(scol.type()).to.equal(3);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);
    });

    it("should have synced the database", function () {
      // First validate that there are no leftovers.
      connectToSlave();
      testDBDoesExist(dbName);
    });

    describe("content of an existing collection", function () {

      it("should have replicated the documents", function () {
        connectToSlave();
        expect(db._collection(docColName).count()).to.equal(100);
      });

    });
  });

  describe(`In ${dbName} database`, function () {

    it("should have synced the document collection", function () {
      connectToMaster();
      db._useDatabase(dbName);
      // First Part Create Collection
      let mcol = db._collection(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      db._useDatabase(dbName);
      testCollectionExists(docColName);
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);
    });

    it("should have synced the edge collection", function () {
      connectToMaster();
      db._useDatabase(dbName);

      // First Part Create Collection
      let mcol = db._collection(edgeColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      db._useDatabase(dbName);

      // Validate it is created properly
      testCollectionExists(edgeColName);
      let scol = db._collection(edgeColName);
      expect(scol.type()).to.equal(3);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);
    });

    describe("content of an existing collection", function () {

      it("should have replicated the documents", function () {
        connectToSlave();
        db._useDatabase(dbName);
        expect(db._collection(docColName).count()).to.equal(100);
      });

    });
  });
});

describe('Test switch off and restart replication', function() {

  before(function() {
    cleanUp();

    startReplication();

    fillMasterWithInitialData();
  });

  after(cleanUp);

  describe('in _system database', function() {

    beforeEach(function() {
      connectToSlave();
      if (!replication.globalApplier.state().state.running) {
        startReplication();
      }
      connectToMaster();
    });

    after(function() {
      connectToSlave();
      stopReplication();
      try {
        db._drop("UnittestOtherCollection");
      } catch (e) {}

      try {
        db._drop("UnittestOtherCollectionIdx");
      } catch (e) {}

      connectToMaster();
      try {
        db._drop("UnittestOtherCollection");
      } catch (e) {}

      try {
        db._drop("UnittestOtherCollectionIdx");
      } catch (e) {}
    });

    it('should replicate offline creation / deletion of collection', function() {
      const col = "UnittestOtherCollection";

      stopReplication();

      connectToMaster();

      let mcol = db._create(col);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      startReplication();

      connectToSlave();
      testCollectionExists(col);
      let scol = db._collection(col);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      compareIndexes(scol.getIndexes(), mIdxs, true);

      // Second part. Delete collection

      stopReplication();

      connectToMaster();
      db._drop(col);
      testCollectionDoesNotExists(col);

      startReplication();

      connectToSlave();
      testCollectionDoesNotExists(col);
    });

    it('should replicate offline creation of an index', function () {
      const col = "UnittestOtherCollectionIdx";

      connectToMaster();

      db._create(col);

      waitForReplication();
      stopReplication();

      connectToMaster();
      let mcol = db._collection(col);
      let omidx = mcol.getIndexes();
      mcol.ensureHashIndex('value');

      let midxs = mcol.getIndexes();

      startReplication();

      connectToSlave();
      let scol = db._collection(col);
      let sidxs = scol.getIndexes();
      compareIndexes(sidxs, midxs, true);
      compareIndexes(sidxs, omidx, false);

      connectToMaster();
      db._drop(col);
    });

    it('should replicate offline insert of documents', function () {
      stopReplication();

      connectToMaster();

      testCollectionExists(docColName);

      let docs = [];

      for (let val = 101; val < 200; ++val) {
        docs.push({value: val});
      }

      let mcol = db._collection(docColName);
      mcol.save(docs);
      let mcount = mcol.count();
      let mchksm = mcol.checksum(true, true);

      startReplication();

      connectToSlave();

      let scol = db._collection(docColName);
      let scount = scol.count();
      let schksm = scol.checksum(true, true);

      expect(scount).to.equal(mcount);
      expect(schksm.checksum).to.equal(mchksm.checksum);
    });
  });
});
