/* global describe, it, beforeEach, afterEach, it, clean, before, after */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for general graph options cluster
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const arangodb = require('@arangodb');
const request = require('@arangodb/request');
const graph = require('@arangodb/general-graph');
const extend = require('lodash').extend;
let endpoint = {};

const db = arangodb.db;

describe('General graph creation', function () {

  const gn = 'UnitTestGraph';
  const vn = 'UnitTestVertices';
  const en = 'UnitTestEdges';
  const on = 'UnitTestOrphans';

  const vn2 = 'UnitTestVerticesOther';
  const en2 = 'UnitTestEdgesOther';
  const on2 = 'UnitTestOrphansOther';
  const vn3 = 'UnitTestVerticesModified';

  const clean = () => {
    try {
      request['delete'](`/_api/gharial/${gn}?dropCollections=true`);
    } catch (ignore) {
    }

    expect(db._collection(vn)).to.not.exist;
    expect(db._collection(en)).to.not.exist;
    expect(db._collection(on)).to.not.exist;

    expect(db._collection(vn2)).to.not.exist;
    expect(db._collection(en2)).to.not.exist;
    expect(db._collection(on2)).to.not.exist;
    expect(db._collection(vn3)).to.not.exist;
  };

  before(clean);
  after(clean);

  describe('with defaults', function () {

    before(function() {
      let rel = graph._relation(en, vn, vn);
      let body = {
        orphanCollections: [on],
        edgeDefinitions: [rel],
        name: gn,
        isSmart: false
      };

      // Create with default options
      let res = request.post(`/_api/gharial`, {
        body: JSON.stringify(body)
      });
      expect(res.statusCode).to.equal(202);

      // Do we need to wait here?

      // Validate all collections get created
      expect(db._collection(vn)).to.exist;
      expect(db._collection(en)).to.exist;
      expect(db._collection(on)).to.exist;

      // Validate create later collections to not exist
      expect(db._collection(vn2)).to.not.exist;
      expect(db._collection(en2)).to.not.exist;
      expect(db._collection(on2)).to.not.exist;
    });

    after(clean);

    describe('during graph construction', function () {

      describe('replication factor and minimal replication factor', function () {

        it('should be stored in the internal document', function () {
          let gdoc = db._collection('_graphs').document(gn);
          expect(gdoc.replicationFactor).to.equal(1);
          expect(gdoc.minReplicationFactor).to.equal(1);
        });

        it('should be 1 for vertex collection', function () {
          let props = db._collection(vn).properties();
          expect(props.replicationFactor).to.equal(1);
          expect(props.minReplicationFactor).to.equal(1);
        });

        it('should be 1 for edge collection', function () {
          let props = db._collection(en).properties();
          expect(props.replicationFactor).to.equal(1);
          expect(props.minReplicationFactor).to.equal(1);
        });

        it('should be 1 for orphan collection', function () {
          let props = db._collection(on).properties();
          expect(props.replicationFactor).to.equal(1);
          expect(props.minReplicationFactor).to.equal(1);
        });

      });

      describe('number of shards', function () {

        it('should be stored in the internal document', function () {
          let gdoc = db._collection('_graphs').document(gn);
          expect(gdoc.numberOfShards).to.equal(1);
        });

        it('should be 1 for vertex collection', function () {
          let props = db._collection(vn).properties();
          expect(props.numberOfShards).to.equal(1);
        });

        it('should be 1 for edge collection', function () {
          let props = db._collection(en).properties();
          expect(props.numberOfShards).to.equal(1);
        });

        it('should be 1 for orphan collection', function () {
          let props = db._collection(on).properties();
          expect(props.numberOfShards).to.equal(1);
        });

      });

    });

    describe('adding collections later', function () {

      before(function() {
        let body = {
          to: [vn2],
          from: [vn2],
          collection: en2
        };
        let res = request.post(`/_api/gharial/${gn}/edge`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        body = {
          collection: on2
        };
        res = request.post(`/_api/gharial/${gn}/vertex`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        // Do we need to wait here?

        // Validate create later collections to now exist
        expect(db._collection(vn2)).to.exist;
        expect(db._collection(en2)).to.exist;
        expect(db._collection(on2)).to.exist;
      });

      describe('replication factor', function () {

        it('should be 1 for vertex collection', function () {
          let props = db._collection(vn2).properties();
          expect(props.replicationFactor).to.equal(1);
        });

        it('should be 1 for edge collection', function () {
          let props = db._collection(en2).properties();
          expect(props.replicationFactor).to.equal(1);
        });

        it('should be 1 for orphan collection', function () {
          let props = db._collection(on2).properties();
          expect(props.replicationFactor).to.equal(1);
        });

      });

      describe('number of shards', function () {

        it('should be 1 for vertex collection', function () {
          let props = db._collection(vn2).properties();
          expect(props.numberOfShards).to.equal(1);
        });

        it('should be 1 for edge collection', function () {
          let props = db._collection(en2).properties();
          expect(props.numberOfShards).to.equal(1);
        });

        it('should be 1 for orphan collection', function () {
          let props = db._collection(on2).properties();
          expect(props.numberOfShards).to.equal(1);
        });

      });

    });

    describe('modify edge definition', function () {

      before(function() {
        // We modify the first relation by adding a new vertex collection

        let body = {
          from: [vn],
          to: [vn3],
          collection: en
        };

        let res = request.put(`/_api/gharial/${gn}/edge/${en}`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        expect(db._collection(vn3)).to.exist;
      });

      describe('replication factor', function () {

        it(`should be 1 for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.replicationFactor).to.equal(1);
        });

      });

      describe('number of shards', function () {

        it(`should be 1 for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.numberOfShards).to.equal(1);
        });

      });

    });

  });

  describe('with startup options', function () {
    const replicationFactor = 2;
    const minReplicationFactor = 2;
    const numberOfShards = 3;

    before(function() {
      const options = {
        replicationFactor,
        minReplicationFactor,
        numberOfShards
      };
      let rel = graph._relation(en, vn, vn);

      let body = {
        orphanCollections: [on],
        edgeDefinitions: [rel],
        name: gn,
        isSmart: false,
        options
      };

      let res = request.post(`/_api/gharial`, {
        body: JSON.stringify(body)
      });

      expect(res.statusCode).to.equal(202);

      // Do we need to wait here?

      // Validate all collections get created
      expect(db._collection(vn)).to.exist;
      expect(db._collection(en)).to.exist;
      expect(db._collection(on)).to.exist;

      // Validate create later collections to not exist
      expect(db._collection(vn2)).to.not.exist;
      expect(db._collection(en2)).to.not.exist;
      expect(db._collection(on2)).to.not.exist;
    });

    after(clean);

    describe('during graph construction', function () {

      describe('replication factor', function () {

        it('should be stored in the internal document', function () {
          let gdoc = db._collection('_graphs').document(gn);
          expect(gdoc.replicationFactor).to.equal(replicationFactor);
          expect(gdoc.minReplicationFactor).to.equal(minReplicationFactor);
        });

        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

        it(`should be ${replicationFactor} for edge collection`, function () {
          let props = db._collection(en).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

        it(`should be ${replicationFactor} for orphan collection`, function () {
          let props = db._collection(on).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

      });

      describe('number of shards', function () {

        it('should be stored in the internal document', function () {
          let gdoc = db._collection('_graphs').document(gn);
          expect(gdoc.numberOfShards).to.equal(numberOfShards);
        });

        it(`should be ${numberOfShards} for vertex collection`, function () {
          let props = db._collection(vn).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

        it(`should be ${numberOfShards} for edge collection`, function () {
          let props = db._collection(en).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

        it(`should be ${numberOfShards} for orphan collection`, function () {
          let props = db._collection(on).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

      });

    });

    describe('adding collections later', function () {

      before(function() {
        let body = {
          to: [vn2],
          from: [vn2],
          collection: en2
        };
        let res = request.post(`/_api/gharial/${gn}/edge`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        body = {
          collection: on2
        };

        res = request.post(`/_api/gharial/${gn}/vertex`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        // Do we need to wait here?

        // Validate create later collections to now exist
        expect(db._collection(vn2)).to.exist;
        expect(db._collection(en2)).to.exist;
        expect(db._collection(on2)).to.exist;
      });

      describe('replication factor', function () {

        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

        it(`should be ${replicationFactor} for edge collection`, function () {
          let props = db._collection(en2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

        it(`should be ${replicationFactor} for orphan collection`, function () {
          let props = db._collection(on2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

      });

      describe('number of shards', function () {

        it(`should be ${numberOfShards} for vertex collection`, function () {
          let props = db._collection(vn2).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

        it(`should be ${numberOfShards} for edge collection`, function () {
          let props = db._collection(en2).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

        it(`should be ${numberOfShards} for orphan collection`, function () {
          let props = db._collection(on2).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

      });

    });

    describe('modify edge definition', function () {

      before(function() {
        // We modify the first relation by adding a new vertex collection

        let body = {
          from: [vn],
          to: [vn3],
          collection: en
        };

        let res = request.put(`/_api/gharial/${gn}/edge/${en}`, extend(endpoint, {
          body: JSON.stringify(body)
        }));

        expect(res.statusCode).to.equal(202);

        expect(db._collection(vn3)).to.exist;
      });

      describe('replication factor', function () {

        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
          expect(props.minReplicationFactor).to.equal(minReplicationFactor);
        });

      });

      describe('number of shards', function () {

        it(`should be ${numberOfShards} for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });

      });

    });


  });

});
