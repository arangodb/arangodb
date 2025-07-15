/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author ChatGPT
// //////////////////////////////////////////////////////////////////////////////

//'use strict';
//
//const expect = require('chai').expect;
//const db = require('internal').db;
//const arango = require('internal').arango;
//
//const testDbName = 'test_ddl_db';
//let connectionHandle = arango.getConnectionHandle();
//const IM = global.instanceManager;
//const { instanceRole } = require("@arangodb/testutils/instance");
//
//
//describe.skip('DDL Operations: Database Create and Drop', function() {
//  before(() => {
//    // Ensure test db does not exist before starting
//    try { db._dropDatabase(testDbName); } catch (e) {}
//  });
//
//  it('should create and drop a database, asserting existence in between', function() {
//    IM.debugSetFailAt('BlockSchedulerMediumQueue', instanceRole.dbServer);
//    try {
//      // Create database
//      let created = db._createDatabase(testDbName, {replicationFactor: 1});
//      expect(created).to.equal(true, 'Database should be created');
//
//      // Switch to new database and check
//      db._useDatabase(testDbName);
//      expect(db._name()).to.equal(testDbName, 'Should be using the new database');
//
//      // Switch back to _system
//      db._useDatabase('_system');
//      // List databases and check
//      let dbs = db._databases();
//      expect(dbs).to.include(testDbName, 'Database should exist after creation');
//
//      // Drop database
//      let dropped = db._dropDatabase(testDbName);
//      expect(dropped).to.equal(true, 'Database should be dropped');
//
//      // List databases and check
//      dbs = db._databases();
//      expect(dbs).to.not.include(testDbName, 'Database should not exist after dropping');
//    } finally {
//      IM.debugClearFailAt();
//    }
//  });
//
//  after(() => {
//    // Clean up in case test failed
//    try { db._useDatabase('_system'); db._dropDatabase(testDbName); } catch (e) {}
//    arango.connectHandle(connectionHandle);
//  });
//});
//
//describe('DDL Operations: Collection Create and Drop', function() {
//  const testCollectionName = 'test_ddl_collection';
//
//  before(() => {
//    // Ensure test db and collection do not exist before starting
//    try { db._dropDatabase(testDbName); } catch (e) {}
//    db._createDatabase(testDbName);
//    db._useDatabase(testDbName);
//    try { db._drop(testCollectionName); } catch (e) {}
//    db._useDatabase('_system');
//  });
//
//  it('should create and drop a collection, asserting existence in between', function() {
//    db._useDatabase(testDbName);
//    IM.debugSetFailAt('BlockSchedulerMediumQueue', instanceRole.dbServer);
//    try {
//      // Create collection
//      let col = db._create(testCollectionName);
//      expect(col.name()).to.equal(testCollectionName, 'Collection should be created');
//
//      // List collections and check
//      let cols = db._collections().map(c => c.name());
//      expect(cols).to.include(testCollectionName, 'Collection should exist after creation');
//
//      // Drop collection
//      let dropped = db._drop(testCollectionName);
//      expect(dropped).to.equal(true, 'Collection should be dropped');
//
//      // List collections and check
//      cols = db._collections().map(c => c.name());
//      expect(cols).to.not.include(testCollectionName, 'Collection should not exist after dropping');
//    } finally {
//      IM.debugClearFailAt();
//    }
//    db._useDatabase('_system');
//  });
//
//  it('should create a persistent index on attribute a in the collection', function() {
//    db._useDatabase(testDbName);
//    // Ensure collection exists
//    if (!db._collection(testCollectionName)) {
//      db._create(testCollectionName);
//    }
//    IM.debugSetFailAt('BlockSchedulerMediumQueue', instanceRole.dbServer);
//    try {
//      // Create persistent index on attribute 'a'
//      let idx = db[testCollectionName].ensureIndex({ type: 'persistent', fields: ['a'] });
//      expect(idx).to.have.property('type', 'persistent');
//      expect(idx.fields).to.deep.equal(['a']);
//      // List indexes and check
//      let indexes = db[testCollectionName].getIndexes();
//      let found = indexes.find(i => i.type === 'persistent' && JSON.stringify(i.fields) === JSON.stringify(['a']));
//      expect(found).to.not.be.undefined;
//    } finally {
//      IM.debugClearFailAt();
//    }
//    db._useDatabase('_system');
//  });
//
//  it('should create an arangosearch view on attribute a of the collection', function() {
//    db._useDatabase(testDbName);
//    // Ensure collection exists
//    if (!db._collection(testCollectionName)) {
//      db._create(testCollectionName);
//    }
//    const testViewName = 'test_ddl_view';
//    // Clean up any previous view
//    try { db._dropView(testViewName); } catch (e) {}
//    IM.debugSetFailAt('BlockSchedulerMediumQueue', instanceRole.dbServer);
//    try {
//      // Create arangosearch view with a link to the collection on attribute 'a'
//      let view = db._createView(testViewName, 'arangosearch', {
//        links: {
//          [testCollectionName]: { fields: { a: {} } }
//        }
//      });
//      expect(view).to.have.property('name', testViewName);
//      expect(view).to.have.property('type', 'arangosearch');
//      // Check the view exists
//      let found = db._view(testViewName);
//      expect(found).to.not.be.undefined;
//      // Check the view properties
//      let props = found.properties();
//      expect(props).to.have.property('links');
//      expect(props.links).to.have.property(testCollectionName);
//      expect(props.links[testCollectionName].fields).to.have.property('a');
//    } finally {
//      IM.debugClearFailAt();
//    }
//    db._useDatabase('_system');
//  });
//
//  after(() => {
//    // Clean up in case test failed
//    try { db._useDatabase(testDbName); db._drop(testCollectionName); } catch (e) {}
//    db._useDatabase('_system');
//    try { db._dropDatabase(testDbName); } catch (e) {}
//    arango.connectHandle(connectionHandle);
//  });
//});
//
//describe('AQL Operations: Join Two Collections', function() {
//  const aqlCol1 = 'aql_ddl_col1';
//  const aqlCol2 = 'aql_ddl_col2';
//  const numDocs = 10;
//
//  before(() => {
//    // Drop collections if they exist
//    try { db._drop(aqlCol1); } catch (e) {}
//    try { db._drop(aqlCol2); } catch (e) {}
//    // Create collections with 3 shards each
//    db._create(aqlCol1, { numberOfShards: 3 });
//    db._create(aqlCol2, { numberOfShards: 3 });
//    // Fill collections with random small documents
//    for (let i = 0; i < numDocs; ++i) {
//      db[aqlCol1].save({ value: Math.floor(Math.random() * 100), idx: i });
//      db[aqlCol2].save({ value: Math.floor(Math.random() * 100), idx: i });
//    }
//  });
//
//  it('should join two collections using AQL', function() {
//    IM.debugSetFailAt('RestAqlHandler::BlockSchedulerMediumQueue', instanceRole.dbServer);
//    IM.debugSetFailAt('RemoteExecutor::UnblockSchedulerMediumQueue', instanceRole.dbServer);
//    IM.debugSetFailAt('RemoteBlock::forceReloadUserManager', instanceRole.coordinator);
//    try {
//      // Simple join on idx
//      let query = `FOR d1 IN ${aqlCol1} FOR d2 IN ${aqlCol2} FILTER d1.idx == d2.idx RETURN {d1, d2}`;
//      db._explain(query);
//      let result = db._query(query).toArray();
//      expect(result.length).to.equal(numDocs);
//      for (let i = 0; i < numDocs; ++i) {
//        expect(result[i]).to.have.property('d1');
//        expect(result[i]).to.have.property('d2');
//        expect(result[i].d1.idx).to.equal(result[i].d2.idx);
//      }
//    } finally {
//      IM.debugClearFailAt();
//    }
//  });
//
//  after(() => {
//    try { db._drop(aqlCol1); } catch (e) {}
//    try { db._drop(aqlCol2); } catch (e) {}
//  });
//});