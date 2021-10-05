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
      request["delete"](`/_api/gharial/${gn}?dropCollections=true`);
    } catch (ignore) {}

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

    before(function () {
      let rel = graph._relation(en, vn, vn);
      let body = {
        orphanCollections: [on],
        edgeDefinitions: [rel],
        name: gn,
        options,
        isSmart: false,
      };
      // Create with default options
      let res = request.post(`/_api/gharial`, {
        body: JSON.stringify(body),
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
        let body = {
          to: [vn2],
          from: [vn2],
          collection: en2,
        };
        let res = request.post(
          `/_api/gharial/${gn}/edge`,
          extend(endpoint, {
            body: JSON.stringify(body),
          })
        );

        expect(res.statusCode).to.equal(202);

        body = {
          collection: on2,
        };
        res = request.post(
          `/_api/gharial/${gn}/vertex`,
          extend(endpoint, {
            body: JSON.stringify(body),
          })
        );

        expect(res.statusCode).to.equal(202);

        // Do we need to wait here?

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

        let body = {
          from: [vn],
          to: [vn3],
          collection: en,
        };

        let res = request.put(
          `/_api/gharial/${gn}/edge/${en}`,
          extend(endpoint, {
            body: JSON.stringify(body),
          })
        );

        expect(res.statusCode).to.equal(202);

        expect(db._collection(vn3)).to.exist;
      });

      it(
        "should be honored for vertex collection",
        validateProperties.bind(this, vn3)
      );
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
