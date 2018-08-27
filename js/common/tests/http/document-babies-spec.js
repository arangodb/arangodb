/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, beforeEach, afterEach, it */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;

const arangodb = require('@arangodb');
const request = require('@arangodb/request');

const ERRORS = arangodb.errors;
const db = arangodb.db;
const wait = require('internal').wait;
const extend = require('lodash').extend;

const errorHeader = 'x-arango-error-codes';
const uniqueCode = ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
const invalidCode = ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code;
const keyBadCode = ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code;

var createOptions = { waitForSync: false,
                      replicationFactor: 2,
                      numberOfShards: 2 };
// Note that the cluster options are ignored in single server

let endpoint = {};

describe('babies collection document', function () {
  const cn = 'UnitTestsCollectionBasics';
  let collection = null;

  beforeEach(function () {
    db._drop(cn);
    collection = db._create(cn, createOptions, "document",
                            {waitForSyncReplication: true});
  });

  afterEach(function () {
    if (collection) {
      collection.unload();
      collection.drop();
      collection = null;
    }

    wait(0.0);
  });

  describe('basics', function () {
    it('insert remove multi (few)', function () {
      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify([{}, {}, {}])
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(3);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request.put('/_api/simple/remove-by-keys', extend(endpoint, {
        body: JSON.stringify({
          keys: ids,
          collection: cn
        })
      }));

      expect(req.statusCode).to.equal(200);
      expect(collection.count()).to.equal(0);
    });

    it('insert remove multi by DELETE (few)', function () {
      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify([{}, {}, {}])
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(3);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request['delete']('/_api/document/' + cn,
        extend(endpoint, { body: JSON.stringify(ids) }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(0);
    });

    it('insert remove multi (many)', function () {
      let l = [];

      for (let i = 0; i < 10000; i++) {
        l.push({
          value: i
        });
      }

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request.put('/_api/simple/remove-by-keys', extend(endpoint, {
        body: JSON.stringify({
          keys: ids,
          collection: cn
        })
      }));

      expect(req.statusCode).to.equal(200);
      expect(collection.count()).to.equal(0);
    });

    it('insert remove multi (many) by DELETE', function () {
      let l = [];

      for (let i = 0; i < 10000; i++) {
        l.push({
          value: i
        });
      }

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request['delete']('/_api/document/' + cn,
        extend(endpoint, { body: JSON.stringify(ids) }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(0);
    });

    it('insert with key remove multi (few)', function () {
      let l = [{
        _key: 'a'
      }, {
        _key: 'b'
      }, {
        _key: 'c'
      }];

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request.put('/_api/simple/remove-by-keys', extend(endpoint, {
        body: JSON.stringify({
          keys: ids,
          collection: cn
        })
      }));

      expect(req.statusCode).to.equal(200);
      expect(collection.count()).to.equal(0);
    });

    it('insert with key remove multi (few) by DELETE', function () {
      let l = [{
        _key: 'a'
      }, {
        _key: 'b'
      }, {
        _key: 'c'
      }];

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request['delete']('/_api/document/' + cn,
        extend(endpoint, { body: JSON.stringify(ids) }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(0);
    });

    it('insert with key remove multi (many)', function () {
      let l = [];

      for (let i = 0; i < 10000; i++) {
        l.push({
          _key: 'K' + i,
          value: i
        });
      }

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request.put('/_api/simple/remove-by-keys', extend(endpoint, {
        body: JSON.stringify({
          keys: ids,
          collection: cn
        })
      }));

      expect(req.statusCode).to.equal(200);
      expect(collection.count()).to.equal(0);
    });

    it('insert with key remove multi (many) by DELETE', function () {
      let l = [];

      for (let i = 0; i < 10000; i++) {
        l.push({
          _key: 'K' + i,
          value: i
        });
      }

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l.length);

      let result = JSON.parse(req.rawBody);
      let ids = result.map(function (x) {
        return x._key;
      });

      req = request['delete']('/_api/document/' + cn,
        extend(endpoint, { body: JSON.stringify(ids) }));

      expect(req.statusCode).to.equal(202);
      expect(collection.count()).to.equal(0);
    });

    it('insert error unique constraint', function () {
      collection.insert([{
        _key: 'a'
      }]);

      let l = [{
        _key: 'b' // new
      }, {
        _key: 'a' // already there
      }];

      let req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);

      let b = JSON.parse(req.rawBody);

      expect(b).to.be.an('array');
      expect(b.length).to.equal(2);
      expect(b[0]._key).to.equal('b');
      expect(b[1].error).to.equal(true);
      expect(b[1].errorNum).to.equal(uniqueCode);

      // Check header error codes
      let headers = req.headers;
      expect(headers).to.have.property(errorHeader);
      let errorCodes = JSON.parse(headers[errorHeader]);
      expect(errorCodes).to.have.property(uniqueCode);
      expect(errorCodes[uniqueCode], 1);

      expect(collection.count()).to.equal(2);

      l = [{
        _key: 'a' // already there
      }, {
        _key: 'c' // new
      }];

      req = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l)
      }));

      expect(req.statusCode).to.equal(202);

      b = JSON.parse(req.rawBody);

      expect(b).to.be.an('array');
      expect(b.length).to.equal(2);
      expect(b[0].error).to.equal(true);
      expect(b[0].errorNum).to.equal(uniqueCode);
      expect(b[1]._key).to.equal('c');
      expect(collection.count()).to.equal(3);

      // Check header error codes
      headers = req.headers;
      expect(headers).to.have.property(errorHeader);
      errorCodes = JSON.parse(headers[errorHeader]);
      expect(errorCodes).to.have.property(uniqueCode);
      expect(errorCodes[uniqueCode], 1);
    });

    it('insert error bad key', function () {
      let l = [null, false, true, 1, -1, {},
        []
      ];

      l.forEach(function (k) {
        let m = [{
          _key: 'a'
        }, {
          _key: k
        }, {
          _key: 'b'
        }];

        let req = request.post('/_api/document/' + cn, extend(endpoint, {
          body: JSON.stringify(m)
        }));

        expect(req.statusCode).to.equal(202);

        let b = JSON.parse(req.rawBody);
        expect(b).to.be.an('array');
        expect(b.length).to.equal(3);
        // The first and the last should work
        expect(b[0]._key).to.equal('a');
        expect(b[2]._key).to.equal('b');

        // The second should fail
        expect(b[1].error).to.equal(true);
        expect(b[1].errorNum).to.equal(keyBadCode);

        // Check header error codes
        let headers = req.headers;
        expect(headers).to.have.property(errorHeader);
        let errorCodes = JSON.parse(headers[errorHeader]);
        expect(errorCodes).to.have.property(keyBadCode);
        expect(errorCodes[keyBadCode], 1);

        expect(collection.count()).to.equal(2);

        collection.remove('a');
        collection.remove('b');
      });

      expect(collection.count()).to.equal(0);
    });

    it('replace multi', function () {
      let l1 = [{
        value: 1
      }, {
        value: 1
      }, {
        value: 1
      }];

      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l1)
      }));

      expect(req1.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b1 = JSON.parse(req1.rawBody);

      let l2 = [{
        value: 2
      }, {
        value: 2
      }, {
        value: 2
      }];

      for (let i = 0; i < l1.length; ++i) {
        l2[i]._key = b1[i]._key;
        l2[i]._rev = b1[i]._rev;
      }

      let req2 = request.put('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l2)
        }));

      expect(req2.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b2 = JSON.parse(req2.rawBody);

      let docs3 = collection.toArray();

      expect(docs3.length).to.equal(l1.length);

      for (let i = 0; i < docs3.length; i++) {
        expect(docs3[i].value).to.equal(2);
      }

      let docs4 = collection.remove(b1);
      expect(docs4.length).to.equal(3);
      for (let i = 0; i < docs3.length; i++) {
        expect(docs4[i].error).to.equal(true);
        expect(docs4[i].errorNum).to.equal(ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      expect(collection.count()).to.equal(3);
      collection.remove(b2);
      expect(collection.count()).to.equal(0);
    });

    it('update multi', function () {
      let l1 = [{
        value: 1
      }, {
        value: 1
      }, {
        value: 1
      }];

      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l1)
      }));

      expect(req1.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b1 = JSON.parse(req1.rawBody);

      let l2 = [{
        value: 2
      }, {
        value: 2
      }, {
        value: 2
      }];

      for (let i = 0; i < l1.length; ++i) {
        l2[i]._key = b1[i]._key;
        l2[i]._rev = b1[i]._rev;
      }

      let req2 = request.patch('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l2)
        }));

      expect(req2.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b2 = JSON.parse(req2.rawBody);

      let docs3 = collection.toArray();

      expect(docs3.length).to.equal(l1.length);

      for (let i = 0; i < docs3.length; i++) {
        expect(docs3[i].value).to.equal(2);
      }

      let docs4 = collection.remove(b1);
      expect(docs4.length).to.equal(3);
      for (let i = 0; i < docs3.length; i++) {
        expect(docs4[i].error).to.equal(true);
        expect(docs4[i].errorNum).to.equal(ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      expect(collection.count()).to.equal(3);
      collection.remove(b2);
      expect(collection.count()).to.equal(0);
    });

    it('replace multi precondition', function () {
      let l1 = [{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }];

      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l1)
      }));

      expect(req1.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b1 = JSON.parse(req1.rawBody);

      let l2 = [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }];

      for (let i = 0; i < l1.length; ++i) {
        l2[i]._key = b1[i]._key;
        l2[i]._rev = b1[i]._rev;
      }

      let req2 = request.put('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l2)
        }));

      expect(req2.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      JSON.parse(req2.rawBody);

      let l3 = [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }];

      for (let i = 0; i < l1.length; ++i) {
        l3[i]._key = b1[i]._key;
        l3[i]._rev = b1[i]._rev;
      }

      let req3 = request.put('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l3)
        }));
      // Babies only have positive result codes
      expect(req3.statusCode).to.equal(202);

      let b3 = JSON.parse(req3.rawBody);
      for (let i = 0; i < l3.length; ++i) {
        expect(b3[i].error).to.equal(true);
        expect(b3[i].errorNum).to.equal(ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      expect(collection.count()).to.equal(l1.length);
    });

    it('update multi precondition', function () {
      let l1 = [{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }];

      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify(l1)
      }));

      expect(req1.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      let b1 = JSON.parse(req1.rawBody);

      let l2 = [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }];

      for (let i = 0; i < l1.length; ++i) {
        l2[i]._key = b1[i]._key;
        l2[i]._rev = b1[i]._rev;
      }

      let req2 = request.patch('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l2)
        }));

      expect(req2.statusCode).to.equal(202);
      expect(collection.count()).to.equal(l1.length);

      JSON.parse(req2.rawBody);

      let l3 = [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }];

      for (let i = 0; i < l1.length; ++i) {
        l3[i]._key = b1[i]._key;
        l3[i]._rev = b1[i]._rev;
      }

      let req3 = request.patch('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify(l3)
        }));

      // Babies only have positive result codes
      expect(req3.statusCode).to.equal(202);

      let b3 = JSON.parse(req3.rawBody);
      for (let i = 0; i < l3.length; ++i) {
        expect(b3[i].error).to.equal(true);
        expect(b3[i].errorNum).to.equal(ERRORS.ERROR_ARANGO_CONFLICT.code);
      }
      expect(collection.count()).to.equal(l1.length);
    });

    it('invalid document type', function () {
      let values1 = [null, false, true, 1, 'abc', [],
        [1, 2, 3]
      ];

      values1.forEach(function (x) {
        let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
          body: JSON.stringify([x])
        }));

        expect(req1.statusCode).to.equal(202);

        let b = JSON.parse(req1.rawBody);
        expect(b).to.be.an('array');
        expect(b.length).to.equal(1);

        expect(b[0].errorNum).to.equal(invalidCode);
        expect(b[0].error).to.equal(true);

        // Check header error codes
        let headers = req1.headers;
        expect(headers).to.have.property(errorHeader);
        let errorCodes = JSON.parse(headers[errorHeader]);
        expect(errorCodes).to.have.property(invalidCode);
        expect(errorCodes[invalidCode], 1);
      });
    });

    it('multiple errors', function () {
      collection.save({_key: 'a'});

      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify([{
          _key: 'b' // valid
        },
          true, // type invalid
          {
            _key: 'a' // unique violated
          }, {
            _key: 'c' // valid
          }, {
            _key: 'b' // unique violated
          }, [
            // type invalid
          ], {
            _key: 'd' // valid
          } ])
      }));

      expect(req1.statusCode).to.equal(202);

      let b = JSON.parse(req1.rawBody);
      expect(b).to.be.an('array');
      expect(b.length).to.equal(7);
      // Check the valid ones
      expect(b[0]._key).to.equal('b');
      expect(b[3]._key).to.equal('c');
      expect(b[6]._key).to.equal('d');

      // Check type invalid
      expect(b[1].error).to.equal(true);
      expect(b[1].errorNum).to.equal(invalidCode);
      expect(b[5].error).to.equal(true);
      expect(b[5].errorNum).to.equal(invalidCode);

      // Check unique violated 
      expect(b[2].error).to.equal(true);
      expect(b[2].errorNum).to.equal(uniqueCode);
      expect(b[4].error).to.equal(true);
      expect(b[4].errorNum).to.equal(uniqueCode);

      expect(collection.count()).to.equal(4);

      // Check header error codes
      let headers = req1.headers;
      expect(headers).to.have.property(errorHeader);
      let errorCodes = JSON.parse(headers[errorHeader]);
      expect(errorCodes).to.have.property(invalidCode);
      expect(errorCodes[invalidCode], 2);

      expect(errorCodes).to.have.property(uniqueCode);
      expect(errorCodes[uniqueCode], 2);
    });
  });

  describe('old and new', function () {
    it('create multi, return new', function () {
      let req1 = request.post('/_api/document/' + cn, extend(endpoint, {
        body: JSON.stringify([{
          'Hallo': 12
        }])
      }));

      expect(req1.statusCode).to.equal(202);

      let b1 = JSON.parse(req1.rawBody);

      expect(b1).to.be.instanceof(Array);
      expect(b1).to.have.lengthOf(1);
      expect(b1[0]).to.be.a('object');
      expect(Object.keys(b1[0])).to.have.lengthOf(3);
      expect(b1[0]._id).to.be.a('string');
      expect(b1[0]._key).to.be.a('string');
      expect(b1[0]._rev).to.be.a('string');

      let req2 = request.post('/_api/document/' + cn + '?returnNew=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 12
          }])
        }));

      expect(req2.statusCode).to.equal(202);

      let b2 = JSON.parse(req2.rawBody);

      expect(b2).to.be.instanceof(Array);
      expect(b2).to.have.lengthOf(1);
      expect(b2[0]).to.be.a('object');
      expect(Object.keys(b2[0])).to.have.lengthOf(4);
      expect(b2[0]._id).to.be.a('string');
      expect(b2[0]._key).to.be.a('string');
      expect(b2[0]._rev).to.be.a('string');
      expect(b2[0]['new']).to.be.a('object');
      expect(Object.keys(b2[0]['new'])).to.have.lengthOf(4);
      expect(b2[0]['new'].Hallo).to.equal(12);

      let req3 = request.post('/_api/document/' + cn + '?returnNew=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 12
          }])
        }));

      expect(req3.statusCode).to.equal(202);

      let b3 = JSON.parse(req3.rawBody);

      expect(b3).to.be.instanceof(Array);
      expect(b3).to.have.lengthOf(1);
      expect(b3[0]).to.be.a('object');
      expect(Object.keys(b3[0])).to.have.lengthOf(3);
      expect(b3[0]._id).to.be.a('string');
      expect(b3[0]._key).to.be.a('string');
      expect(b3[0]._rev).to.be.a('string');
    });

    it('replace multi, return old and new', function () {
      var res = collection.insert({
        'Hallo': 12
      });

      let req1 = request.put('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 13,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req1.statusCode).to.equal(202);

      let b1 = JSON.parse(req1.rawBody);

      expect(b1).to.be.instanceof(Array);
      expect(b1).to.have.lengthOf(1);
      expect(b1[0]).to.be.a('object');
      expect(Object.keys(b1[0])).to.have.lengthOf(4);
      expect(b1[0]._id).to.be.a('string');
      expect(b1[0]._key).to.be.a('string');
      expect(b1[0]._rev).to.be.a('string');
      expect(b1[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req2 = request.put('/_api/document/' + cn + '?returnOld=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 13,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req2.statusCode).to.equal(202);

      let b2 = JSON.parse(req2.rawBody);

      expect(b2).to.be.instanceof(Array);
      expect(b2).to.have.lengthOf(1);
      expect(b2[0]).to.be.a('object');
      expect(Object.keys(b2[0])).to.have.lengthOf(5);
      expect(b2[0]._id).to.be.a('string');
      expect(b2[0]._key).to.be.a('string');
      expect(b2[0]._rev).to.be.a('string');
      expect(b2[0]._oldRev).to.be.a('string');
      expect(b2[0].old).to.be.a('object');
      expect(Object.keys(b2[0].old)).to.have.lengthOf(4);
      expect(b2[0].old.Hallo).to.equal(12);

      res = collection.insert({
        'Hallo': 12
      });

      let req3 = request.put('/_api/document/' + cn + '?returnOld=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 14,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req3.statusCode).to.equal(202);

      let b3 = JSON.parse(req3.rawBody);

      expect(b3).to.be.instanceof(Array);
      expect(b3).to.have.lengthOf(1);
      expect(b3[0]).to.be.a('object');
      expect(Object.keys(b3[0])).to.have.lengthOf(4);
      expect(b3[0]._id).to.be.a('string');
      expect(b3[0]._key).to.be.a('string');
      expect(b3[0]._rev).to.be.a('string');
      expect(b3[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req4 = request.put('/_api/document/' + cn + '?returnNew=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 14,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req4.statusCode).to.equal(202);

      let b4 = JSON.parse(req4.rawBody);

      expect(b4).to.be.instanceof(Array);
      expect(b4).to.have.lengthOf(1);
      expect(b4[0]).to.be.a('object');
      expect(Object.keys(b4[0])).to.have.lengthOf(5);
      expect(b4[0]._id).to.be.a('string');
      expect(b4[0]._key).to.be.a('string');
      expect(b4[0]._rev).to.be.a('string');
      expect(b4[0]._oldRev).to.be.a('string');
      expect(b4[0]['new']).to.be.a('object');
      expect(Object.keys(b4[0]['new'])).to.have.lengthOf(4);
      expect(b4[0]['new'].Hallo).to.equal(14);

      res = collection.insert({
        'Hallo': 12
      });

      let req5 = request.put('/_api/document/' + cn + '?returnNew=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 15,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req5.statusCode).to.equal(202);

      let b5 = JSON.parse(req5.rawBody);

      expect(b5).to.be.instanceof(Array);
      expect(b5).to.have.lengthOf(1);
      expect(b5[0]).to.be.a('object');
      expect(Object.keys(b5[0])).to.have.lengthOf(4);
      expect(b5[0]._id).to.be.a('string');
      expect(b5[0]._key).to.be.a('string');
      expect(b5[0]._rev).to.be.a('string');
      expect(b5[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req6 = request.put('/_api/document/' + cn + '?returnNew=true&returnOld=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 16,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req6.statusCode).to.equal(202);

      let b6 = JSON.parse(req6.rawBody);

      expect(b6).to.be.instanceof(Array);
      expect(b6).to.have.lengthOf(1);
      expect(b6[0]).to.be.a('object');
      expect(Object.keys(b6[0])).to.have.lengthOf(6);
      expect(b6[0]._id).to.be.a('string');
      expect(b6[0]._key).to.be.a('string');
      expect(b6[0]._rev).to.be.a('string');
      expect(b6[0]._oldRev).to.be.a('string');
      expect(b6[0].old).to.be.a('object');
      expect(Object.keys(b6[0])).to.have.lengthOf(6);
      expect(b6[0].old.Hallo).to.equal(12);
      expect(b6[0]['new']).to.be.a('object');
      expect(Object.keys(b6[0]['new'])).to.have.lengthOf(4);
      expect(b6[0]['new'].Hallo).to.equal(16);

      res = collection.insert({
        'Hallo': 12
      });

      let req7 = request.put('/_api/document/' + cn + '?returnNew=false&returnOld=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 17,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req7.statusCode).to.equal(202);

      let b7 = JSON.parse(req2.rawBody);

      expect(b7).to.be.instanceof(Array);
      expect(b7).to.have.lengthOf(1);
      expect(b7[0]).to.be.a('object');
      expect(Object.keys(b7[0])).to.have.lengthOf(5);
      expect(b7[0]._id).to.be.a('string');
      expect(b7[0]._key).to.be.a('string');
      expect(b7[0]._rev).to.be.a('string');
      expect(b7[0]._oldRev).to.be.a('string');
    });

    it('update multi, return old and new', function () {
      var res = collection.insert({
        'Hallo': 12
      });

      let req1 = request.patch('/_api/document/' + cn + '?ignoreRevs=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 13,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req1.statusCode).to.equal(202);

      let b1 = JSON.parse(req1.rawBody);

      expect(b1).to.be.instanceof(Array);
      expect(b1).to.have.lengthOf(1);
      expect(b1[0]).to.be.a('object');
      expect(Object.keys(b1[0])).to.have.lengthOf(4);
      expect(b1[0]._id).to.be.a('string');
      expect(b1[0]._key).to.be.a('string');
      expect(b1[0]._rev).to.be.a('string');
      expect(b1[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req2 = request.patch('/_api/document/' + cn + '?returnOld=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 13,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req2.statusCode).to.equal(202);

      let b2 = JSON.parse(req2.rawBody);

      expect(b2).to.be.instanceof(Array);
      expect(b2).to.have.lengthOf(1);
      expect(b2[0]).to.be.a('object');
      expect(Object.keys(b2[0])).to.have.lengthOf(5);
      expect(b2[0]._id).to.be.a('string');
      expect(b2[0]._key).to.be.a('string');
      expect(b2[0]._rev).to.be.a('string');
      expect(b2[0]._oldRev).to.be.a('string');
      expect(b2[0].old).to.be.a('object');
      expect(Object.keys(b2[0].old)).to.have.lengthOf(4);
      expect(b2[0].old.Hallo).to.equal(12);

      res = collection.insert({
        'Hallo': 12
      });

      let req3 = request.patch('/_api/document/' + cn + '?returnOld=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 14,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req3.statusCode).to.equal(202);

      let b3 = JSON.parse(req3.rawBody);

      expect(b3).to.be.instanceof(Array);
      expect(b3).to.have.lengthOf(1);
      expect(b3[0]).to.be.a('object');
      expect(Object.keys(b3[0])).to.have.lengthOf(4);
      expect(b3[0]._id).to.be.a('string');
      expect(b3[0]._key).to.be.a('string');
      expect(b3[0]._rev).to.be.a('string');
      expect(b3[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req4 = request.patch('/_api/document/' + cn + '?returnNew=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 14,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req4.statusCode).to.equal(202);

      let b4 = JSON.parse(req4.rawBody);

      expect(b4).to.be.instanceof(Array);
      expect(b4).to.have.lengthOf(1);
      expect(b4[0]).to.be.a('object');
      expect(Object.keys(b4[0])).to.have.lengthOf(5);
      expect(b4[0]._id).to.be.a('string');
      expect(b4[0]._key).to.be.a('string');
      expect(b4[0]._rev).to.be.a('string');
      expect(b4[0]._oldRev).to.be.a('string');
      expect(b4[0]['new']).to.be.a('object');
      expect(Object.keys(b4[0]['new'])).to.have.lengthOf(4);
      expect(b4[0]['new'].Hallo).to.equal(14);

      res = collection.insert({
        'Hallo': 12
      });

      let req5 = request.patch('/_api/document/' + cn + '?returnNew=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 15,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req5.statusCode).to.equal(202);

      let b5 = JSON.parse(req5.rawBody);

      expect(b5).to.be.instanceof(Array);
      expect(b5).to.have.lengthOf(1);
      expect(b5[0]).to.be.a('object');
      expect(Object.keys(b5[0])).to.have.lengthOf(4);
      expect(b5[0]._id).to.be.a('string');
      expect(b5[0]._key).to.be.a('string');
      expect(b5[0]._rev).to.be.a('string');
      expect(b5[0]._oldRev).to.be.a('string');

      res = collection.insert({
        'Hallo': 12
      });

      let req6 = request.patch('/_api/document/' + cn + '?returnNew=true&returnOld=true',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 16,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req6.statusCode).to.equal(202);

      let b6 = JSON.parse(req6.rawBody);

      expect(b6).to.be.instanceof(Array);
      expect(b6).to.have.lengthOf(1);
      expect(b6[0]).to.be.a('object');
      expect(Object.keys(b6[0])).to.have.lengthOf(6);
      expect(b6[0]._id).to.be.a('string');
      expect(b6[0]._key).to.be.a('string');
      expect(b6[0]._rev).to.be.a('string');
      expect(b6[0]._oldRev).to.be.a('string');
      expect(b6[0].old).to.be.a('object');
      expect(Object.keys(b6[0])).to.have.lengthOf(6);
      expect(b6[0].old.Hallo).to.equal(12);
      expect(b6[0]['new']).to.be.a('object');
      expect(Object.keys(b6[0]['new'])).to.have.lengthOf(4);
      expect(b6[0]['new'].Hallo).to.equal(16);

      res = collection.insert({
        'Hallo': 12
      });

      let req7 = request.patch('/_api/document/' + cn + '?returnNew=false&returnOld=false',
        extend(endpoint, {
          body: JSON.stringify([{
            'Hallo': 17,
            '_key': res._key,
            '_rev': res._rev
          }])
        }));

      expect(req7.statusCode).to.equal(202);

      let b7 = JSON.parse(req2.rawBody);

      expect(b7).to.be.instanceof(Array);
      expect(b7).to.have.lengthOf(1);
      expect(b7[0]).to.be.a('object');
      expect(Object.keys(b7[0])).to.have.lengthOf(5);
      expect(b7[0]._id).to.be.a('string');
      expect(b7[0]._key).to.be.a('string');
      expect(b7[0]._rev).to.be.a('string');
      expect(b7[0]._oldRev).to.be.a('string');
    });
  });

  describe('overwrite', function () {
    let base_url = '/_api/document/' + cn;
    it('overwrite once', function () {
      let url1 = base_url;
      let req1 = request.post(url1, extend(endpoint, {
        body: JSON.stringify([{
          'Hallo': 12
        }])
      }));
      let b1 = JSON.parse(req1.rawBody);
      let res1 = b1[0];

      let url2 = base_url + '?overwrite=true&returnOld=true';
      let req2 = request.post(url2, extend(endpoint, {
        body: JSON.stringify([{
          '_key': res1._key,
          'ulf': 42
        }])
      }));
      let b2 = JSON.parse(req2.rawBody);
      let res2 = b2[0];

      expect(req2.statusCode).to.equal(202);
      expect(res2._key).to.equal(res1._key);
      expect(res2._oldRev).to.equal(res1._rev);
      expect(res2.old.Hallo).to.equal(12);


    });

    it('overwrite multi', function () {
      let url1 = base_url;
      let req1 = request.post(url1, extend(endpoint, {
        body: JSON.stringify([{
          'Hallo': 12
        }])
      }));
      let b1 = JSON.parse(req1.rawBody);
      let res1 = b1[0];
      let key1 = res1._key;

      let url2 = base_url + '?overwrite=true&returnOld=true&returnNew=true';
      let req2 = request.post(url2, extend(endpoint, {
        body: JSON.stringify([
          {
            '_key': key1,
            'ulf': 42
          },{
            '_key': key1,
            'ulf': 32

          },{
            '_key': key1,
            'ulfine': 23
          }
        ])
      }));
      expect(req2.statusCode).to.equal(202);
      let b2 = JSON.parse(req2.rawBody);

      expect(b2).to.be.instanceof(Array);
      expect(b2).to.have.lengthOf(3);
      expect(b2[0]).to.be.a('object');
      expect(b2[1]).to.be.a('object');
      expect(b2[2]).to.be.a('object');
      expect(b2[0]._key).to.be.a('string');
      expect(b2[0]._key).to.equal(key1);
      expect(b2[1]._key).to.equal(key1);
      expect(b2[2]._key).to.equal(key1);
      expect(b2[1]._rev).to.equal(b2[2].old._rev);
      expect(b2[2].old._rev).to.equal(b2[1].new._rev);
      expect(b2[2].new.ulfine).to.equal(23);
      expect(b2[2].new.ulf).to.equal(undefined);

    });
  }); // overwrite - end
});
