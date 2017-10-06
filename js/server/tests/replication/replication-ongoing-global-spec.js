/* global describe, it */

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
const sinon = require('sinon');
const arangodb = require("@arangodb");
const replication = require("@arangodb/replication");
const errors = arangodb.errors;
const db = arangodb.db;

const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

const username = "root";
const password = "";

const dbName = "UnitTestDB";
const docColName = "UnitTestDocs";
const edgeColName = "UnitTestEdges";

// Flag if we need to reconnect.
let onMaster = true;

const waitForReplication = function() {
  // TODO Implement properly, based on ticks
  require("internal").wait(3);
};

// We always connect to _system DB because it is always present.
// You do not need to monitor any state.
const connectToMaster = function() {
  if (!onMaster) {
    arango.reconnect(masterEndpoint, "_system", username, password);
    db._flushCache();
    onMaster = true;
  } else {
    db._useDatabase("_system");
  }
};

const connectToSlave = function() {
  if (onMaster) {
    arango.reconnect(slaveEndpoint, "_system", username, password);
    db._flushCache();
    onMaster = false;
  } else {
    db._useDatabase("_system");
  }
};

const testCollectionExists = function(name) {
  expect(db._collections().indexOf(name)).to.not.equal(-1, `Collection ${name} does not exist although it should`);
};

const testCollectionDoesNotExists = function(name) {
  expect(db._collections().indexOf(name)).to.equal(-1, `Collection ${name} does exist although it should not`);
};

const testDBDoesExist= function(name) {
  expect(db._databases().indexOf(name)).to.not.equal(-1, `Database ${name} does not exist although it should`);
};

const testDBDoesNotExist = function(name) {
  expect(db._databases().indexOf(name)).to.equal(-1, `Database ${name} does exist although it should not`);
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

const cleanUp = function() {
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

  while (replication.applier.state().state.running) {
    internal.wait(0.1, false);
  }

  cleanUpAllData();

  connectToMaster();
  cleanUpAllData();
};

describe('Global Repliction on a fresh boot', function () {

  before(function() {
    cleanUp();
    // Setup global replication
    connectToSlave();

    let config = {
      endpoint: masterEndpoint,
      username: username,
      password: password,
      verbose: true,
      /* TODO Do we need those parameters?
      includeSystem: true,
      restrictType: "",
      restrictCollections: [],
      keepBarrier: false
      */
    };

    replication.setupReplicationGlobal(config);
  });

  after(cleanUp);


  describe("In _system database", function () {

    before(function() {
      db._useDatabase("_system");
    })

    it("should create and drop an empty document collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._create(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionExists(docColName);
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      expect(scol.getIndexes()).to.deep.equal(mIdxs);

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
      expect(scol.getIndexes()).to.deep.equal(mIdxs);

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
        testCollectionNotExists(docColName);
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
        db._collection(docColName).ensureHashIndex("value");

        let mIdx = db._collection(docColName).getIndexes();

        waitForReplication();
        connectToSlave();

        let sIdx = db._collection(docColName).getIndexes();
        expect(sIdx).to.deep.equal(sIdx);
      });
    });
  });

  describe(`In ${dbName} database`, function () {

    before(function() {
      connectToMaster();
      testDBDoesNotExist(dbName);

      db._createDatabase(dbName);
      waitForReplication();

      connectToSlave();
      testDBDoesExist(dbName);

      connectToMaster();
      db._useDatabase(dbName);
    });

    after(function() {
      connectToMaster();
      // Flip always uses _system
      db._dropDatabase(dbName);

      waitForReplication();
      connectToSlave();
      testDBDoesExist(dbName);
    });

    it("should create and drop an empty document collection", function () {
      connectToMaster();
      // First Part Create Collection
      let mcol = db._create(docColName);
      let mProps = mcol.properties();
      let mIdxs = mcol.getIndexes();

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionExists(docColName);
      let scol = db._collection(docColName);
      expect(scol.type()).to.equal(2);
      expect(scol.properties()).to.deep.equal(mProps);
      expect(scol.getIndexes()).to.deep.equal(mIdxs);

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
      expect(scol.getIndexes()).to.deep.equal(mIdxs);

      connectToMaster();
      // Second Part Drop it again
      db._drop(edgeColName);
      testCollectionDoesNotExists(edgeColName);

      connectToSlave();
      // Validate it is created properly
      waitForReplication();
      testCollectionDoesNotExists(edgeColName);
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
        testCollectionNotExists(docColName);
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
        db._collection(docColName).ensureHashIndex("value");

        let mIdx = db._collection(docColName).getIndexes();

        waitForReplication();
        connectToSlave();

        let sIdx = db._collection(docColName).getIndexes();
        expect(sIdx).to.deep.equal(sIdx);
      });
    });

  });
});
