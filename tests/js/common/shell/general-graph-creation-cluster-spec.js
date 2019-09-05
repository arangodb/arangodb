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

const arangodb = require('@arangodb');
const expect = require('chai').expect;
const graph = require("@arangodb/general-graph");
const _ = require("lodash");
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
      graph._drop(gn, true);
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
    let g;

    before(function () {
      let rel = graph._relation(en, vn, vn);
      // Create with default options
      g = graph._create(gn, [rel], [on]);

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
          expect(gdoc.replicationFactor).to.equal(1);
        });

        it('should be 1 for vertex collection', function () {
          let props = db._collection(vn).properties();
          expect(props.replicationFactor).to.equal(1);
        });

        it('should be 1 for edge collection', function () {
          let props = db._collection(en).properties();
          expect(props.replicationFactor).to.equal(1);
        });

        it('should be 1 for orphan collection', function () {
          let props = db._collection(on).properties();
          expect(props.replicationFactor).to.equal(1);
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

      before(function () {
        let rel = graph._relation(en2, vn2, vn2);
        g._extendEdgeDefinitions(rel);
        g._addVertexCollection(on2);

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
      before(function () {
        // We modify the first relation by adding a new vertex collection
        let rel = graph._relation(en, vn, vn3);
        g._editEdgeDefinitions(rel);

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
    let g;
    const replicationFactor = 2;
    const numberOfShards = 3;

    before(function () {
      const options = {
        replicationFactor,
        numberOfShards
      };
      let rel = graph._relation(en, vn, vn);
      // Create with default options
      g = graph._create(gn, [rel], [on], options);

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
        });

        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
        });

        it(`should be ${replicationFactor} for edge collection`, function () {
          let props = db._collection(en).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
        });

        it(`should be ${replicationFactor} for orphan collection`, function () {
          let props = db._collection(on).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
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

      before(function () {
        let rel = graph._relation(en2, vn2, vn2);
        g._extendEdgeDefinitions(rel);
        g._addVertexCollection(on2);

        // Validate create later collections to now exist
        expect(db._collection(vn2)).to.exist;
        expect(db._collection(en2)).to.exist;
        expect(db._collection(on2)).to.exist;
      });

      describe('replication factor', function () {

        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
        });

        it(`should be ${replicationFactor} for edge collection`, function () {
          let props = db._collection(en2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
        });

        it(`should be ${replicationFactor} for orphan collection`, function () {
          let props = db._collection(on2).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
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
      before(function () {
        // We modify the first relation by adding a new vertex collection
        let rel = graph._relation(en, vn, vn3);
        g._editEdgeDefinitions(rel);
        expect(db._collection(vn3)).to.exist;
      });

      describe('replication factor', function () {
        it(`should be ${replicationFactor} for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.replicationFactor).to.equal(replicationFactor);
        });
      });

      describe('number of shards', function () {
        it(`should be ${numberOfShards} for vertex collection`, function () {
          let props = db._collection(vn3).properties();
          expect(props.numberOfShards).to.equal(numberOfShards);
        });
      });
    });

    describe('modify vertices', function () {
      it(`remove a vertex collection from the graph definition and also drop the collection`, function () {
        expect(db[on].name() === on);
        g._removeVertexCollection(on, true);

        // check that the collection is really dropped
        // using collections list
        var collections = db._collections();
        var found = false;
        _.each(collections, function (collection) {
          if (collection.name() === on) {
            found = true;
          }
        });

        expect(found).to.be.false;
      });
    });

  });
});
