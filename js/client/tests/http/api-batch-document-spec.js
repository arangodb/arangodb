/*jshint globalstrict:true, strict:true, esnext: true */
/* global describe, beforeEach, afterEach, it */
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

  beforeEach(() => {
    db._create(colName);
    db._create(edgeColName);
  });

  afterEach(() => {
    db._drop(colName);
    db._drop(edgeColName);
  });

  describe('read document suite', () => {
    const n = 20;
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

    it('read one document at a time', () => {
      internal.print('Iterating over ' + docsByVal.length + ' docs');
      docsByVal.forEach((doc, i) => {
        internal.print({i, doc});
        let body;
        const response = readDocs(body = {
          data: [{_key: doc._key}],
          options: {},
        });
        internal.print({body, response});
        expect(response.statusCode).to.equal(200);
        expect(response.json).to.be.an('object').that.has.all.keys(['error', 'result']);
        expect(response.json).to.be.an('object').that.has.all.keys(['error']); // just a check that the expect doesn't allow superfluous keys; remove this line when it failed once
        expect(response.json.error).to.equal(false);
        expect(response.json.result).to.deep.equal([doc]);
      });

    });
  });

});
