/*jshint globalstrict:true, strict:true, esnext: true */
/* global describe, beforeEach, afterEach, it, xit, print */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const internal = require('internal');
const request = require('@arangodb/request');

const db = internal.db;

describe('_api/batch/document', () => {
  const colName = 'UnitTestDocument';
  const edgeColName = 'UnitTestEdges';

  const batchRequest = operation => collection => body => {
    return request.post(`/_api/batch/document/${collection}/${operation}`, {
      body: JSON.stringify(body)
    });
  };

  const metaOfDoc = doc => ({
    _key: doc._key,
    _rev: doc._rev,
    _id: doc._id,
  });

  // Consider these variations (working requests) when writing tests:
  // - how many documents are requested
  // - how does the pattern look like
  //   - does it contain an attribute more than _key?
  //   - does it contain _rev? (*should* work like regular attribute, but is
  //     still handled specially in a lot of code!)
  // - how does options look like?
  //   which options are omitted/default, true, false?
  //   - splitTransaction
  //   - silent
  //   - returnOld
  //   - returnNew
  //   - waitForSync
  //   - checkGraphs
  //   - graphName

  // In addition to that, check failures:
  // - send invalid requests:
  //   - invalid structure (e.g. request body object or string instead of array)
  //   - invalid data and invalid options
  //     - missing required fields
  //     - fields that exist in other, but not this, request (e.g. waitForSync
  //       for /read)
  //     - fields that don't exist at all
  //     - invalid values (e.g. returnOld: 0 or silent: "true")
  // - ask for nonexistent documents (with and without splitTransaction)
  //   - mix asking for existing and non-existing documents
  //   - try different kinds of non-existent:
  //     non-existent _key, wrong _rev, wrong regular attribute
  // (the following tests may not belong in this suite:
  //   - try illegal modifications
  //     - inserts/replace with invalid / missing attributes
  //     - update/replace read-only attributes
  // )

  // Tests for checkGraph:
  // - when removing a vertex, removals of incident edges over all edge
  //   definitions
  // - when inserting/updating/replacing an edge, validity checks of _from and
  //   _to over edge definitions (on all graphs!)
  // Tests for graphName (in addition to above checks):
  // - when edges are inserted, validity check that an edge definition exists
  //   in this graph
  // - when vertices are inserted, validity check that the vertex collection
  //   is either part of an edge or orphan collection in this graph

  beforeEach(() => {
    db._create(colName);
    db._create(edgeColName);
  });

  afterEach(() => {
    db._drop(colName);
    db._drop(edgeColName);
  });

  // READ IS NOT YET IMPLEMENTED! Replace the xit() calls by it() when it's done.
  describe('read document suite', () => {
    const n = 4;
    let docsByKey;
    let docsByVal;

    const readDocs = batchRequest('read')(colName);

    beforeEach(() => {
      docsByKey = {};
      docsByVal = [];
      const docs = db._query(
        'FOR i IN 0..@n INSERT {val: i} IN @@col RETURN NEW',
        {n: n-1, '@col': colName}
      ).toArray();
      expect(docs).to.be.an('array').and.to.have.lengthOf(n);
      docs.forEach((doc, i) => {
        docsByVal[i] = doc;
        docsByKey[doc._key] = doc;
      });
    });

    afterEach(() => {
      db[colName].truncate();
    });

    xit('read one document at a time', () => {
      docsByVal.forEach(doc => {
        const response = readDocs({
          data: [{pattern: {_key: doc._key}}],
          options: {},
        });
        expect(response.statusCode).to.equal(200);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
        expect(response.json.error).to.equal(false);
        expect(response.json.result).to.deep.equal([{doc}]);
      });
    });

    xit('read all documents in a batch', () => {
      let body;
      const response = readDocs(body = {
        data: docsByVal.map(doc => ({pattern: {_key: doc._key}})),
        options: {},
      });

      expect(response.statusCode).to.equal(200);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({doc})));

    });
  });

  describe('remove document suite', () => {
    const n = 3;
    let docsByKey;
    let docsByVal;

    const removeDocs = batchRequest('remove')(colName);

    beforeEach(() => {
      docsByKey = {};
      docsByVal = [];
      const docs = db._query(
        'FOR i IN 0..@n INSERT {val: i} IN @@col RETURN NEW',
        {n: n-1, '@col': colName}
      ).toArray();
      expect(docs).to.be.an('array').and.to.have.lengthOf(n);
      docs.forEach((doc, i) => {
        docsByVal[i] = doc;
        docsByKey[doc._key] = doc;
      });
    });

    afterEach(() => {
      db[colName].truncate();
    });

    it('remove one document at a time', () => {
      print("############################################");
      print('remove one document at a time - this shoulds show ' + n + ' times ');
      let docsLeft = n;
      docsByVal.forEach(doc => {
        const response = removeDocs({
          data: [{pattern: {_key: doc._key}}],
          options: {},
        });

        print("############################################");
        print(response.json);

        expect(response.statusCode).to.equal(202);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
        expect(response.json.error).to.equal(false);
        expect(response.json.result).to.deep.equal([{old: metaOfDoc(doc)}]); // test return old
        --docsLeft;
        expect(db[colName].count()).to.equal(docsLeft);
      });
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('remove one document at a time - fail', () => {
      print("############################################");
      print('remove one document at a time - this should show ' + n + ' times - fail');
      print('Could failed documents contain the key? This would allow better error messages');
      let docsLeft = n;
      docsByVal.forEach(doc => {
        const response = removeDocs({
          data: [{pattern: {_key: "DerGondelUlf"}}],
          options: {},
        });

        print("############################################");
        print(response.json);

        expect(response.statusCode).to.equal(202);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
        expect(response.json.error).to.equal(true);
        expect(response.json.result).to.deep.equal([{ "errorNum" : 1202, "errorMessage" : "document not found" }]);
        expect(db[colName].count()).to.equal(docsLeft);
      });
      expect(db[colName].count()).to.equal(3);
      print("############################################");
    });

    it('remove all documents in a batch', () => {
      print("############################################");
      print('remove all documents in a batch');
      let body;
      const response = removeDocs(body = {
        data: docsByVal.map(doc => ({pattern: {_key: doc._key}})),
        options: {},
      });

      print("############################################");
      print(response.json);

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({old: metaOfDoc(doc)})));
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('remove all documents in a batch - silent', () => {
      print("############################################");
      print('remove all documents in a batch - silent ');
      let body;
      const response = removeDocs(body = {
        data: docsByVal.map(doc => ({pattern: {_key: doc._key}})),
        options: { silent: true},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({})));
      //expect(response.json.result).to.deep.equal([]); //empty ?!
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('remove all documents in a batch - fail', () => {
      print("############################################");
      print('remove all documents in a batch - fail');
      let body;
      const response = removeDocs(body = {
        //data: docsByVal.map(doc => ({pattern: {_key: doc._key}})),
        data: docsByVal.map(doc => ({pattern: {_key: docsByVal[0]._key}})),
        options: {},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
      expect(response.json.error).to.equal(true);
      //expect(response.json.result).to.deep.equal(docsByVal.map(metaOfDoc));
      expect(db[colName].count()).to.equal(2);
      print("############################################");
    });

  }); // remove document - end



  describe('update document suite', () => {
    const n = 3;
    let docsByKey;
    let docsByVal;

    const updateDocs = batchRequest('update')(colName);

    beforeEach(() => {
      docsByKey = {};
      docsByVal = [];
      const docs = db._query(
        'FOR i IN 0..@n INSERT {val: i} IN @@col RETURN NEW',
        {n: n-1, '@col': colName}
      ).toArray();
      expect(docs).to.be.an('array').and.to.have.lengthOf(n);
      docs.forEach((doc, i) => {
        docsByVal[i] = doc;
        docsByKey[doc._key] = doc;
      });
    });

    afterEach(() => {
      db[colName].truncate();
    });

    it('update one document at a time', () => {
      print("############################################");
      print('update one document at a time - this shoulds show ' + n + ' times ');
      let docsLeft = n;
      docsByVal.forEach(doc => {
        const response = updateDocs({
          data: [{pattern: {_key: doc._key},
                  updateDocument: { x : 42 }
                 }
                ],
          options: {},
        });

        print("############################################");
        print(response.json);

        expect(response.statusCode).to.equal(202);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
        expect(response.json.error).to.equal(false);
        expect(response.json.result).to.deep.equal([{old: metaOfDoc(doc)}]); // test return old
        --docsLeft;
        expect(db[colName].count()).to.equal(docsLeft);
      });
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('update one document at a time - fail', () => {
      print("############################################");
      print('update one document at a time - this should show ' + n + ' times - fail');
      print('Could failed documents contain the key? This would allow better error messages');
      let docsLeft = n;
      docsByVal.forEach(doc => {
        const response = updateDocs({
          data: [{pattern: {_key: "DerGondelUlf"},
                  updateDocument: { x : 42 }
                 }
                ],
          options: {}
        });

        print("############################################");
        print(response.json);

        expect(response.statusCode).to.equal(202);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
        expect(response.json.error).to.equal(true);
        expect(response.json.result).to.deep.equal([{ "errorNum" : 1202, "errorMessage" : "document not found" }]);
        expect(db[colName].count()).to.equal(docsLeft);
      });
      expect(db[colName].count()).to.equal(3);
      print("############################################");
    });

    it('update all documents in a batch', () => {
      print("############################################");
      print('update all documents in a batch');
      let body;
      const response = updateDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: doc._key},
          updateDocument: { x : 42 }
        })),
        options: {}
      });

      print("############################################");
      print(response.json);

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({old: metaOfDoc(doc)})));
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('update all documents in a batch - silent', () => {
      print("############################################");
      print('update all documents in a batch - silent ');
      let body;
      const response = updateDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: doc._key},
          updateDocument: { x : 42 }
        })),
        options: { silent: true},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({})));
      //expect(response.json.result).to.deep.equal([]); //empty ?!
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('update all documents in a batch - fail', () => {
      print("############################################");
      print('update all documents in a batch - fail');
      let body;
      const response = updateDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: docsByVal[0]._key},
          updateDocument: { x : 42 }
        })),
        options: {},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
      expect(response.json.error).to.equal(true);
      expect(db[colName].count()).to.equal(2);
      print("############################################");
    });
  }); // update document - end



  describe('replace document suite', () => {
    const n = 3;
    let docsByKey;
    let docsByVal;

    const replaceDocs = batchRequest('replace')(colName);

    beforeEach(() => {
      docsByKey = {};
      docsByVal = [];
      const docs = db._query(
        'FOR i IN 0..@n INSERT {val: i} IN @@col RETURN NEW',
        {n: n-1, '@col': colName}
      ).toArray();
      expect(docs).to.be.an('array').and.to.have.lengthOf(n);
      docs.forEach((doc, i) => {
        docsByVal[i] = doc;
        docsByKey[doc._key] = doc;
      });
    });

    afterEach(() => {
      db[colName].truncate();
    });

    it('replace one document at a time - fail', () => {
      print("############################################");
      print('replace one document at a time - this should show ' + n + ' times - fail');
      print('Could failed documents contain the key? This would allow better error messages');
      let docsLeft = n;
      docsByVal.forEach(doc => {
        const response = replaceDocs({
          data: [{pattern: {_key: "DerGondelUlf"},
                  replaceDocument: { x : 42 }
                 }
                ],
          options: {}
        });

        print("############################################");
        print(response.json);

        expect(response.statusCode).to.equal(202);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
        expect(response.json.error).to.equal(true);
        expect(response.json.result).to.deep.equal([{ "errorNum" : 1202, "errorMessage" : "document not found" }]);
        expect(db[colName].count()).to.equal(docsLeft);
      });
      expect(db[colName].count()).to.equal(3);
      print("############################################");
    });

    it('replace all documents in a batch', () => {
      print("############################################");
      print('replace all documents in a batch');
      let body;
      const response = replaceDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: doc._key},
          replaceDocument: { x : 42 }
        })),
        options: {}
      });

      print("############################################");
      print(response.json);

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({old: metaOfDoc(doc)})));
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('replace all documents in a batch - silent', () => {
      print("############################################");
      print('replace all documents in a batch - silent ');
      let body;
      const response = replaceDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: doc._key},
          replaceDocument: { x : 42 }
        })),
        options: { silent: true},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
      expect(response.json.error).to.equal(false);
      expect(response.json.result).to.deep.equal(docsByVal.map(doc => ({})));
      //expect(response.json.result).to.deep.equal([]); //empty ?!
      expect(db[colName].count()).to.equal(0);
      print("############################################");
    });

    it('replace all documents in a batch - fail', () => {
      print("############################################");
      print('replace all documents in a batch - fail');
      let body;
      const response = replaceDocs(body = {
        data: docsByVal.map(doc => ({
          pattern: {_key: docsByVal[0]._key},
          replaceDocument: { x : 42 }
        })),
        options: {},
      });

      print(response.json);
      print("############################################");

      expect(response.statusCode).to.equal(202);
      expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result', 'errorDataIndex']);
      expect(response.json.error).to.equal(true);
      expect(db[colName].count()).to.equal(2);
      print("############################################");
    });
  }); // replace document - end



}); // _api/batch/document - end
