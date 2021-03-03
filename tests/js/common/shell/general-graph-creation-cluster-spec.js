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

const defaultReplicationFactor = db._properties().replicationFactor;

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

  const testSuite = (options) => {
    // Extract desired values, if absent use defaults
    const replicationFactor =
      options.replicationFactor || defaultReplicationFactor;
    const minReplicationFactor = options.minReplicationFactor || 1;
    const numberOfShards = options.numberOfShards || 1;

    // Assert that we do not tests defaults again if we want to overwrite
    // with specific values
    if (options.hasOwnProperty("replicationFactor")) {
      expect(replicationFactor).to.not.equal(defaultReplicationFactor);
    }
    if (options.hasOwnProperty("minReplicationFactor")) {
      expect(minReplicationFactor).to.not.equal(1);
    }
    if (options.hasOwnProperty("numberOfShards")) {
      expect(numberOfShards).to.not.equal(1);
    }

    const validateProperties = (collectionName) => {
      let props = db._collection(collectionName).properties();
      expect(props.replicationFactor).to.equal(replicationFactor);
      expect(props.minReplicationFactor).to.equal(minReplicationFactor);
      expect(props.numberOfShards).to.equal(numberOfShards);
    };

    let g;

    before(function () {
      let rel = graph._relation(en, vn, vn);
      if (Object.keys(options).length === 0) {
        // Create with default empty options
        g = graph._create(gn, [rel], [on]);
      } else {
        // Create with given options
        g = graph._create(gn, [rel], [on], options);
      }

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

    describe("during graph construction, the options", function () {
      it("should be stored in the internal document", function () {
        let gdoc = db._collection("_graphs").document(gn);
        expect(gdoc.replicationFactor).to.equal(replicationFactor);
        expect(gdoc.minReplicationFactor).to.equal(minReplicationFactor);
        expect(gdoc.numberOfShards).to.equal(numberOfShards);
      });

      it(
        "should be honored for vertex collection",
        validateProperties.bind(this, vn)
      );

      it(
        "should be honored for edge collection",
        validateProperties.bind(this, en)
      );

      it(
        "should be honored for orphan collection",
        validateProperties.bind(this, on)
      );
    });

    describe("adding collections later, options", function () {
      before(function () {
        let rel = graph._relation(en2, vn2, vn2);
        g._extendEdgeDefinitions(rel);
        g._addVertexCollection(on2);

        // Validate create later collections to now exist
        expect(db._collection(vn2)).to.exist;
        expect(db._collection(en2)).to.exist;
        expect(db._collection(on2)).to.exist;
      });

      it(
        "should be honored for vertex collection",
        validateProperties.bind(this, vn2)
      );

      it(
        "should be honored for edge collection",
        validateProperties.bind(this, en2)
      );

      it(
        "should be honored for orphan collection",
        validateProperties.bind(this, on2)
      );
    });

    describe("modify edge definition, options", function () {
      before(function () {
        // We modify the first relation by adding a new vertex collection
        let rel = graph._relation(en, vn, vn3);
        g._editEdgeDefinitions(rel);

        expect(db._collection(vn3)).to.exist;
      });

      it(
        "should be honored for vertex collection",
        validateProperties.bind(this, vn3)
      );
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
  };


  before(clean);
  after(clean);

  describe("with defaults", testSuite.bind(this, {}));

  let opts = { minReplicationFactor: 2 };
  if (defaultReplicationFactor === 1) {
    opts.replicationFactor = 2;
  }
  describe(
    "with min replication factor startup options",
    testSuite.bind(this, opts)
  );

  opts = { numberOfShards: 3 };
  if (defaultReplicationFactor === 2) {
    opts.replicationFactor = 1;
  }
  describe(
    "with replication factor and shards startup options",
    testSuite.bind(this, opts)
  );
});
