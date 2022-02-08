/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, beforeEach, afterEach, it, _ */
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
// / @author Michael Hackstein
// / @author Heiko Kernbach
// //////////////////////////////////////////////////////////////////////////////

const chai = require('chai');
const Joi = require('joi');
const expect = chai.expect;
chai.Assertion.addProperty('does', function () {
  return this;
});

const arangodb = require('@arangodb');
const arango = arangodb.arango;
const gM = require("@arangodb/general-graph");
const isEnterprise = require("internal").isEnterprise();

const ERRORS = arangodb.errors;
const db = arangodb.db;
const internal = require('internal');
const isCluster = internal.isCluster();
const wait = internal.wait;
const url = '/_api/gharial';

describe('_api/gharial', () => {
  const graphName = 'UnitTestGraph';
  const graphName2 = 'UnitTestGraph2';
  const vColName = 'UnitTestVertices';
  const eColName = 'UnitTestRelations';
  const oColName = 'UnitTestOrphans';
  const oColName2 = 'UnitTestOrphansSecond';

  const cleanup = () => {
    try {
      db._drop(vColName);
    } catch (e) {
    }
    try {
      db._drop(eColName);
    } catch (e) {
    }
    try {
      db._drop(oColName);
    } catch (e) {
    }
    try {
      db._drop(oColName2);
    } catch (e) {
    }
    try {
      db._graphs.remove(graphName);
    } catch (e) {
    }
    try {
      db._graphs.remove('knows_graph');
    } catch (e) {
    }
    try {
      db._drop('persons');
    } catch (e) {
    }
    try {
      db._drop('knows');
    } catch (e) {
    }
    try {
      db._drop('knows_2');
    } catch (e) {
    }
  };

  // @brief validates the graph format including all expected properties.
  // Expected values depends on the environment.
  const validateGraphFormat = (
    graph,
    validationProperties
  ) => {
    const isSmart = (validationProperties && validationProperties.isSmart) || false;
    const isDisjoint = (validationProperties && validationProperties.isDisjoint) || false;
    const isSatellite = (validationProperties && validationProperties.isSatellite) || false;
    const hasDetails = (validationProperties && validationProperties.hasDetails) || false;
    const hybridCollections = (validationProperties && validationProperties.hybridCollections) || [];
    const onlySatellitesCreated = (validationProperties && validationProperties.onlySatellitesCreated) || false;
    /*
     * Edge Definition Schema
     */
    let edgeDefinitionSchema;
    if (hasDetails) {
      edgeDefinitionSchema = Joi.object({
        collection: Joi.string().required(),
        from: Joi.array().items(Joi.string()).required(),
        to: Joi.array().items(Joi.string()).required(),
        checksum: Joi.string().required()
      });
    } else {
      edgeDefinitionSchema = Joi.object({
        collection: Joi.string().required(),
        from: Joi.array().items(Joi.string()).required(),
        to: Joi.array().items(Joi.string()).required()
      });
    }
    Object.freeze(edgeDefinitionSchema);

    /*
     * Collection Properties Schema
     */

    // This is always required to be available
    let generalGraphSchema = {
      "_key": Joi.string().required(),
      "_rev": Joi.string().required(),
      "_id": Joi.string().required(),
      name: Joi.string().required(),
      orphanCollections: Joi.array().items(Joi.string()).required(),
      edgeDefinitions: Joi.array().items(edgeDefinitionSchema).required()
    };
    if (hasDetails) {
      generalGraphSchema.checksum = Joi.string().required();
    }

    if (isCluster || isSmart || isSatellite) {
      // Those properties are either:
      // - Required for all graphs which are being created in a cluster
      // - OR SmartGraphs (incl. Disjoint & Hybrid, as SmartGraphs can now be created
      //   in a SingleServer environment as well)
      const distributionGraphSchema = {
        numberOfShards: Joi.number().integer().min(1).required(),
        isSmart: Joi.boolean().required(),
        isSatellite: Joi.boolean().required()
      };

      if (isSatellite) {
        distributionGraphSchema.replicationFactor = Joi.string().valid('satellite').required();
      } else {
        distributionGraphSchema.replicationFactor = Joi.number().integer().min(1).required();
        distributionGraphSchema.minReplicationFactor = Joi.number().integer().min(1).required();
        distributionGraphSchema.writeConcern = Joi.number().integer().min(1).required();
      }

      Object.assign(generalGraphSchema, distributionGraphSchema);
    }

    if (isSmart) {
      // SmartGraph related only
      let smartGraphSchema = {
        smartGraphAttribute: Joi.string().required(),
        isDisjoint: Joi.boolean().required()
      };
      if (isDisjoint) {
        expect(graph.isDisjoint).to.be.true;
      }
      Object.freeze(smartGraphSchema);
      Object.assign(generalGraphSchema, smartGraphSchema);
    }

    if ((isSmart || isSatellite) && !onlySatellitesCreated) {
      let smartOrSatSchema = {
        initial: Joi.string().required(),
        initialCid: Joi.number().integer().min(1).required()
      };
      Object.freeze(smartOrSatSchema);
      Object.assign(generalGraphSchema, smartOrSatSchema);
    }

    if (hasDetails && isSmart && !isSatellite) {
      // This is a special case, means:
      // Combination out of a SmartGraph and additional collections which
      // should be created as SatelliteCollections. In that case the API
      // should expose the collections which are created as satellites as
      // well. Otherwise, it will be an empty array.
      const hybridSmartGraphSchema = {
        satellites: Joi.array().items(Joi.string()).required()
      };

      Object.assign(generalGraphSchema, hybridSmartGraphSchema);

      if (hybridCollections.length > 0) {
        hybridCollections.forEach((hybridCol) => {
          expect(graph.satellites.indexOf(hybridCol)).to.be.greaterThan(-1);
        });
      } else {
        expect(graph.satellites).to.be.an('array');
        expect(graph.satellites.length).to.equal(0);
      }
    }

    // now create the actual joi object out of the completed schema combination
    generalGraphSchema = Joi.object(generalGraphSchema);

    // start schema validation
    const res = generalGraphSchema.validate(graph);
    expect(res.error).to.be.null;
  };

  beforeEach(cleanup);

  afterEach(cleanup);

  describe('graph creation', () => {
    it('should create a graph without orphans', () => {
      const graphDef = {
        "name": graphName,
        "edgeDefinitions": [{
          "collection": eColName,
          "from": [vColName],
          "to": [vColName]
        }
        ],
        "isSmart": false
      };
      expect(db._collection(eColName)).to.be.null;
      expect(db._collection(vColName)).to.be.null;
      let req = arango.POST(url, graphDef);
      expect(req).to.have.keys("error", "code", "graph");
      expect(req.code).to.equal(202);
      expect(req.error).to.be.false;
      validateGraphFormat(req.graph);

      // This is all async give it some time
      do {
        wait(0.1);
        req = arango.GET(url + "/" + graphName);
      } while (req.code !== 200);

      expect(db._collection(eColName)).to.not.be.null;
      expect(db._collection(vColName)).to.not.be.null;

      expect(req).to.have.keys("error", "code", "graph");
      expect(req.code).to.equal(200);
      expect(req.error).to.be.false;
      validateGraphFormat(req.graph);
    });

    it('should create a graph with orphans', () => {
      const graphDef = {
        "name": graphName,
        "edgeDefinitions": [{
          "collection": eColName,
          "from": [vColName],
          "to": [vColName]
        }
        ],
        "orphanCollections": [
          oColName,
          oColName2
        ],
        "isSmart": false
      };
      expect(db._collection(eColName)).to.be.null;
      expect(db._collection(vColName)).to.be.null;
      expect(db._collection(oColName)).to.be.null;
      expect(db._collection(oColName2)).to.be.null;
      let req = arango.POST(url, graphDef);
      expect(req).to.have.keys("error", "code", "graph");
      expect(req.code).to.equal(202);
      expect(req.error).to.be.false;
      validateGraphFormat(req.graph);

      // This is all async give it some time
      do {
        wait(0.1);
        req = arango.GET(url + "/" + graphName);
      } while (req.code !== 200);

      expect(db._collection(eColName)).to.not.be.null;
      expect(db._collection(vColName)).to.not.be.null;
      expect(db._collection(oColName)).to.not.be.null;
      expect(db._collection(oColName2)).to.not.be.null;

      expect(req).to.have.keys("error", "code", "graph");
      expect(req.code).to.equal(200);
      expect(req.error).to.be.false;
      validateGraphFormat(req.graph);
    });

  });

  describe('graph modification test suite', function () {
    const vertexUrl = `${url}/${graphName}/vertex`;
    const edgeUrl = `${url}/${graphName}/edge`;

    beforeEach(() => {
      const graphDef = {
        "name": graphName,
        "edgeDefinitions": [{
          "collection": eColName,
          "from": [vColName],
          "to": [vColName]
        }
        ],
        "isSmart": false
      };
      expect(db._collection(eColName)).to.be.null;
      expect(db._collection(vColName)).to.be.null;
      let req = arango.POST(url, graphDef);

      // Just make sure the graph exists
      do {
        wait(0.1);
        req = arango.GET(url + "/" + graphName);
      } while (req.code !== 200);
    });

    it('should list all graphs in correct format', () => {
      const res = arango.GET(url);
      expect(res.code).to.equal(200);
      expect(res.error).to.be.false;
      expect(res).to.have.keys("error", "code", "graphs");

      res.graphs.map(
        function (graph) {
          return validateGraphFormat(graph);
        }
      );
    });

    it('should be able to add an orphan', () => {
      const res = arango.POST(vertexUrl, {collection: oColName});

      expect(res).to.have.keys("error", "code", "graph");
      expect(res.code).to.equal(202);
      expect(res.error).to.be.false;
      validateGraphFormat(res.graph);
      expect(db._collection(oColName)).to.not.be.null;
    });

    it('should be able to modify edge definition', () => {
      const res = arango.PUT(`${edgeUrl}/${eColName}`, {
        "collection": eColName,
        "from": [vColName, oColName],
        "to": [vColName]
      });
      expect(res).to.have.keys("error", "code", "graph");
      expect(res.code).to.equal(202);
      expect(res.error).to.be.false;
      validateGraphFormat(res.graph);

      expect(db._collection(oColName)).to.not.be.null;
    });
  });

  describe('edge modification test suite', function () {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';

    describe('edge creation (POST)', function () {
      describe('valid positives', function () {
        it('_from and _to vertices are existent and valid', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/bob',
            _to: 'persons/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(202);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });
      });

      describe('false positives', function () {
        it('_from is set but vertex is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/notavailable',
            _to: 'persons/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to is set but vertex is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/bob',
            _to: 'persons/notavailable'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from and _to are set but are not available documents', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/notavailable',
            _to: 'persons/notavailable'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from definition is valid, but collection is not available', () => {
          const examples = require('@arangodb/graph-examples/example-graph');
          const exampleGraphName = 'knows_graph';
          const vName = 'persons';
          const eName = 'knows';
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'xxx/peter',
            _to: 'persons/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from definition is valid, but document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows/', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to definition is valid, but document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _to: 'persons/peterNotExisting',
            _from: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows/', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from && _to definitions are valid, but documents are not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlieNotExisting'
          };

          // get a (any) valid key of an existing edge document
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows/', edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to definition is valid, but collection is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/bob',
            _to: 'xxx/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from && _to definitions are valid, but colletions are not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'xxx/peter',
            _to: 'xxx/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'peter',
            _to: 'persons/charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peter',
            _to: 'charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from && _to attributes are invalid (strings)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'peter',
            _to: 'charlie'
          };
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('empty edge definition', () => {
          const examples = require('@arangodb/graph-examples/example-graph');
          const exampleGraphName = 'knows_graph';
          const vName = 'persons';
          const eName = 'knows';
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {};
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/knows', edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });
      });
    });

    describe('edge updates (PATCH)', function () {
      describe('valid positives', function () {
        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          // get a (any) valid key of an existing edge document
          // and modify it's from's and to's
          const edgeDef = db.knows.any();
          edgeDef._from = 'persons/charlie';
          edgeDef._to = 'persons/charlie';

          let res = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + edgeDef._key, edgeDef);
          // 202 without waitForSync (default)
          expect(res.code).to.equal(202);
          expect(res.error).to.equal(false);
          expect(res.edge._key).to.equal(edgeDef._key);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });
      });

      describe('false positives', function () {
        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/charlie',
            _to: 'charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to vertex collection not part of graph definition', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/charlie',
            _to: 'personsNot/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from vertex collection not part of graph definition', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _to: 'persons/charlie',
            _from: 'personsNot/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _to: 'persons/peterNotExisting',
            _from: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from && _to documents are not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlieNotExisting'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'charlie',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

      });
    });

    describe('edge replacements (PUT)', function () {
      describe('valid positives', function () {
        it('should check that edges can be replaced', () => {
          const examples = require('@arangodb/graph-examples/example-graph');
          const exampleGraphName = 'knows_graph';
          const vName = 'persons';
          const eName = 'knows';
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;
          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;

          const e = db.knows.any();

          const newEdge = Object.assign({}, e);
          newEdge.newAttribute = 'new value';

          const res = arango.PUT(
            `${url}/${exampleGraphName}/edge/${eName}/${e._key}`, newEdge);

          // 202 without waitForSync (default)
          expect(res.code).to.equal(202);
          expect(res.error).to.equal(false);
          expect(res.edge._key).to.equal(e._key);

          expect(db.knows.document(e._key))
            .to.be.an('object')
            .that.has.property('newAttribute')
            .which.equals('new value');
        });

      });

      describe('false positives', function () {
        it('should check that edges can NOT be replaced if their _from or _to' +
          ' attribute is missing', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;
          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;

          const e = db.knows.any();

          let newEdge;
          let newEdges = {};
          newEdge = newEdges['_from missing'] = Object.assign({new: "new"}, e);
          delete newEdge._from;
          newEdge = newEdges['_to missing'] = Object.assign({new: "new"}, e);
          delete newEdge._to;
          newEdge = newEdges['_from and _to missing'] = Object.assign({new: "new"}, e);
          delete newEdge._from;
          delete newEdge._to;


          for (let key in newEdges) {
            if (!newEdges.hasOwnProperty(key)) {
              continue;
            }
            const description = key;
            const newEdge = newEdges[key];

            const res = arango.PUT(
              `${url}/${exampleGraphName}/edge/${eName}/${e._key}`,
              newEdge);

            expect(res.code, description).to.equal(400);
            expect(res.errorNum, description)
              .to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);
          }

          expect(db.knows.document(e._key))
            .to.be.an('object')
            .that.does.not.have.property('new');
        });

        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/charlie',
            _to: 'charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to vertex collection not part of graph definition', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/charlie',
            _to: 'personsNot/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from vertex collection not part of graph definition', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _to: 'persons/charlie',
            _from: 'personsNot/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to document is not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _to: 'persons/peterNotExisting',
            _from: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from && _to documents are not available', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlieNotExisting'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_from attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'charlie',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PATCH(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(400);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

        it('_to attribute is invalid (string)', () => {
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;

          const edgeDef = {
            _from: 'persons/peterNotExisting',
            _to: 'persons/charlie'
          };

          // get a (any) valid key of an existing edge document
          const _key = db.knows.any()._key;
          let req = arango.PUT(url + '/' + exampleGraphName + '/edge/knows/' + _key, edgeDef);
          expect(req.code).to.equal(404);
          expect(req.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;
        });

      });
    });

    describe('edge deletion (DELETE)', function () {
      it('should check if incident edges are deleted with a vertex', () => {
        // vertices
        const alice = 'alice';
        const bob = 'bob';
        const eve = 'eve';

        expect(db._collection(eName)).to.be.null;
        expect(db._collection(vName)).to.be.null;
        // load graph
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        // pre-check that the expected edges are there
        expect(db[eName].all().toArray().length).to.equal(5);

        // delete vertex bob
        const res = arango.DELETE(
          `${url}/${exampleGraphName}/vertex/${vName}/${bob}`
        );

        // check response
        // 202 without waitForSync (default)
        expect(res).to.eql({
          error: false,
          code: 202,
          removed: true
        });

        // check that all edges incident to bob were removed as well
        expect(db[eName].all().toArray().length).to.equal(1);

        // check that the remaining edge is the expected one
        const eveKnowsAlice = db[eName].all().toArray()[0];
        expect(eveKnowsAlice).to.have.all.keys(
          ['_key', '_id', '_rev', '_from', '_to', 'vertex']
        );
        expect(eveKnowsAlice).to.include({
          _from: `${vName}/${eve}`,
          _to: `${vName}/${alice}`,
          vertex: eve
        });
      });

      it('should check that non-graph incident edges are not deleted with a' +
        ' vertex', () => {
        const examples = require('@arangodb/graph-examples/example-graph');
        const exampleGraphName = 'knows_graph';
        const vName = 'persons';
        const eName = 'knows';
        // vertices
        const alice = 'alice';
        const bob = 'bob';
        const charlie = 'charlie';
        const dave = 'dave';
        const eve = 'eve';

        expect(db._collection(eName)).to.be.null;
        expect(db._collection(vName)).to.be.null;
        // load graph
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        const ngEdges = db._create(eColName);
        ngEdges.insert({
          _from: `${vName}/${bob}`,
          _to: `${vName}/${charlie}`,
          name: 'bob->charlie'
        });
        ngEdges.insert({
          _from: `${vName}/${dave}`,
          _to: `${vName}/${bob}`,
          name: 'dave->bob'
        });

        // pre-check that the expected edges are there
        expect(db[eName].all().toArray().length).to.equal(5);

        // delete vertex bob
        const res = arango.DELETE(
          `${url}/${exampleGraphName}/vertex/${vName}/${bob}`
        );

        // check response
        // 202 without waitForSync (default)
        expect(res).to.eql({
          error: false,
          code: 202,
          removed: true
        });

        // check that the edges outside of g are still there
        let remainingEdges = ngEdges.all().toArray();
        expect(remainingEdges.length).to.equal(2);
        expect(remainingEdges.map(x => x.name))
          .to.have.members(['bob->charlie', 'dave->bob']);
      });
    });

    describe('edge to edge suite', function () {
      describe('valid positives', function () {
        it('create edge to another edge, then update, replace & delete', function () {
          // testing gharial api for direct edge to edge linking
          const examples = require('@arangodb/graph-examples/example-graph');
          const exampleGraphName = 'knows_graph';
          const vName = 'persons';
          const eName = 'knows';
          const eName_2 = 'knows_2';
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(eName_2)).to.be.null; // edgec2
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;
          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;

          // add a new edge definition to the knows graph pointing from knows -> knows (edge to edge)
          const G = require('@arangodb/general-graph');
          const graph = G._graph(exampleGraphName);
          graph._extendEdgeDefinitions({collection: "knows_2", from: ["knows_2"], to: ["knows_2"]});

          // vertices
          const alice = 'alice'; // edge from alice to bob exists
          const bob = 'bob';

          //                                     ( A --> B )
          //                                         /\
          //                                          |
          //                                     ( A <-- B )
          const edgeDefBToA = {
            _from: vName + "/" + bob,
            _to: vName + "/" + alice
          };

          const edgeDefAToB = {
            _to: vName + "/" + bob,
            _from: vName + "/" + alice
          };

          // create the actual edge pointing from bob to alice
          let reqB = arango.POST(url + '/' + exampleGraphName + '/edge/' + eName_2, edgeDefBToA);
          expect(reqB.code).to.equal(202);
          let bobToAlice = reqB.edge;

          // create the actual edge pointing from alice to bob
          let reqA = arango.POST(url + '/' + exampleGraphName + '/edge/' + eName_2, edgeDefAToB);
          expect(reqA.code).to.equal(202);
          let aliceToBob = reqA.edge;


          // now create a new edge between the edges from A->B and B->A
          const edgeLinkDef = {
            _from: aliceToBob._id,
            _to: bobToAlice._id
          };

          let reqx = arango.POST(url + '/' + exampleGraphName + '/edge/' + eName_2, edgeLinkDef);

          let newEdge = reqx.edge;
          expect(reqx.code).to.equal(202);

          const updateEdgeLinkDef = {
            _to: aliceToBob._id,
            _from: bobToAlice._id
          };

          // UPDATE that edge
          reqx = arango.PATCH(url + '/' + exampleGraphName + '/edge/' + eName_2 + "/" + newEdge._key, updateEdgeLinkDef);
          newEdge = reqx.edge;
          expect(reqx.code).to.equal(202);

          // REPLACE that edge
          reqx = arango.PUT(url + '/' + exampleGraphName + '/edge/' + eName_2 + "/" + newEdge._key, edgeLinkDef);
          newEdge = reqx.edge;
          expect(reqx.code).to.equal(202);

          // DELETE that edge
          reqx = arango.DELETE(url + '/' + exampleGraphName + '/edge/' + eName_2 + "/" + newEdge._key, {});
          expect(reqx.code).to.equal(202);
        });
      });

      describe('false positives', function () {
        it('create edge to another edge, but not defined within edge definition', function () {
          // testing gharial api for direct edge to edge linking
          const examples = require('@arangodb/graph-examples/example-graph');
          const exampleGraphName = 'knows_graph';
          const vName = 'persons';
          const eName = 'knows';
          expect(db._collection(eName)).to.be.null; // edgec
          expect(db._collection(vName)).to.be.null; // vertexc
          const g = examples.loadGraph(exampleGraphName);
          expect(g).to.not.be.null;
          expect(db._collection(eName)).to.not.be.null;
          expect(db._collection(vName)).to.not.be.null;

          // vertices
          const alice = 'alice'; // edge from alice to bob exists
          const bob = 'bob';

          // get the edge from alice to bob
          const aliceToBob = db.knows.byExample({_from: vName + "/" + alice, _to: vName + "/" + bob}).toArray()[0];

          //                                     ( A --> B )
          //                                         /\
          //                                          |
          //                                     ( A <-- B )
          const edgeDef = {
            _from: vName + "/" + bob,
            _to: vName + "/" + alice
          };

          // create the actual edge pointing from bob to alice
          let req = arango.POST(url + '/' + exampleGraphName + '/edge/' + eName, edgeDef);
          expect(req.code).to.equal(202);
          let bobToAlice = req.edge;

          // now create a new edge between the edges from A->B and B->A
          const edgeLinkDef = {
            _from: aliceToBob._id,
            _to: bobToAlice._id
          };

          let reqx = arango.POST(url + '/' + exampleGraphName + '/edge/' + eName, edgeLinkDef);
          expect(reqx.code).to.equal(400);
          expect(reqx.errorNum).to.equal(ERRORS.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);
          expect(reqx.error).to.equal(true);
        });
      });
    });

    describe('header match verification', function () {
      it('should check if the if-match header is working - positive', () => {
        const examples = require('@arangodb/graph-examples/example-graph');
        const exampleGraphName = 'knows_graph';
        const vName = 'persons';
        const eName = 'knows';
        expect(db._collection(eName)).to.be.null; // edgec
        expect(db._collection(vName)).to.be.null; // vertexc
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        const key = 'bob';
        const doc = db[vName].document(key);
        const revision = doc._rev; // get a valid revision

        let req = arango.GET(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key, {
          headers: {
            'if-match': revision
          }
        });
        expect(req.code).to.equal(200);
        expect(req.edge).to.deep.equal(doc);
      });

      it('should check if the if-match header is working - negative', () => {
        const examples = require('@arangodb/graph-examples/example-graph');
        const exampleGraphName = 'knows_graph';
        const vName = 'persons';
        const eName = 'knows';
        expect(db._collection(eName)).to.be.null; // edgec
        expect(db._collection(vName)).to.be.null; // vertexc
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        const key = 'bob';
        const doc = db[vName].document(key);
        let revision = doc._rev; // get a valid revision
        revision = revision + 'x';

        const revisions = [null, undefined, true, false, revision];

        revisions.forEach(function (rev) {
          let req = arango.GET(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key, {
            'if-match': rev
          });
          expect(req.error).to.equal(true);
          expect(req.code).to.equal(ERRORS.ERROR_HTTP_PRECONDITION_FAILED.code);
          expect(req.errorMessage).to.equal(ERRORS.ERROR_ARANGO_CONFLICT.message);
        });
      });

      it('should check if the if-none-match header is working - positive', () => {
        const examples = require('@arangodb/graph-examples/example-graph');
        const exampleGraphName = 'knows_graph';
        const vName = 'persons';
        const eName = 'knows';
        expect(db._collection(eName)).to.be.null; // edgec
        expect(db._collection(vName)).to.be.null; // vertexc
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        const key = 'bob';
        const doc = db[vName].document(key);
        const revision = doc._rev; // get a valid revision

        let req = arango.GET(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key, {
          'if-none-match': revision
        });
        expect(req.code).to.equal(304);
      });

      it('should check if the if-none-match header is working - negative', () => {
        const examples = require('@arangodb/graph-examples/example-graph');
        const exampleGraphName = 'knows_graph';
        const vName = 'persons';
        const eName = 'knows';
        expect(db._collection(eName)).to.be.null; // edgec
        expect(db._collection(vName)).to.be.null; // vertexc
        const g = examples.loadGraph(exampleGraphName);
        expect(g).to.not.be.null;
        expect(db._collection(eName)).to.not.be.null;
        expect(db._collection(vName)).to.not.be.null;

        const key = 'bob';
        const doc = db[vName].document(key);
        let revision = doc._rev; // get a valid revision
        revision = revision + 'x';

        const revisions = [null, undefined, true, false, revision];

        revisions.forEach(function (rev) {
          let req = arango.GET(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key, {
            'if-none-match': rev
          });
          expect(req.code).to.equal(200);
          expect(req.edge).to.deep.equal(doc);
        });
      });
    });

    describe('_api/gharial testing cross-graph deletes', () => {
      var graph = require("@arangodb/general-graph");

      const cleanup = () => {
        try {
          graph._drop("firstGraph", true);
        } catch (e) {
        }
        try {
          graph._drop("secondGraph", true);
        } catch (e) {
        }
      };

      beforeEach(cleanup);

      afterEach(cleanup);

      it('should also remove an edge in a second graph if connection in first graph is removed', () => {
        // G1 = X ---- e ----> Y
        // G2 = A --^               // A points to e in G1
        //      A ---- b ----> B    // A points to B in G2

        // G1
        var g1 = graph._create("firstGraph",
          graph._edgeDefinitions(
            graph._relation("firstEdge", ["firstFrom"], ["firstTo"])
          )
        );
        var vertexFrom1 = db["firstFrom"].save({});
        var vertexIDFrom1 = vertexFrom1._id;
        var vertexTo1 = db["firstTo"].save({});
        var vertexIDTo1 = vertexTo1._id;

        var edge1 = db["firstEdge"].save(vertexIDTo1, vertexIDFrom1, {_key: "1"});
        var edgeID1 = edge1._id;

        // G2
        var g2 = graph._create("secondGraph",
          graph._edgeDefinitions(
            graph._relation("secondEdge", ["secondFrom"], ["firstEdge"])
          )
        );
        var vertexFrom2 = db["secondFrom"].save({});
        var vertexIDFrom2 = vertexFrom2._id;

        // create edge from G2 to G1 using HTTP API
        const edgeDef = {
          _from: vertexIDFrom2,
          _to: edgeID1
        };

        // create edge pointing from g2 to g1 (edge)
        let req = arango.POST(url + '/' + 'secondGraph' + '/edge/secondEdge', edgeDef);
        expect(req.code).to.equal(202);
        let toBeRemovedEdgeID = req.edge._id;

        // now delete the target edge of g1
        let req2 = arango.DELETE(url + '/' + 'firstGraph' + '/edge/' + edgeID1);
        expect(req2.code).to.equal(202);

        var deleted = false;
        try {
          let deletedEdge = db._document(toBeRemovedEdgeID);
        } catch (e) {
          expect(e.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
          deleted = true;
        }

        expect(deleted).to.equal(true);
      });

      it('should also remove an edge in a second graph if connected vertex in first graph is removed', () => {
        // G1 = X ---- e ----> Y
        // G2 = A -------------^    // A points to Y in G1
        //      A ---- b ----> B    // A points to B in G2

        // G1
        var g1 = graph._create("firstGraph",
          graph._edgeDefinitions(
            graph._relation("firstEdge", ["firstFrom"], ["firstTo"])
          )
        );
        var vertexFrom1 = db["firstFrom"].save({});
        var vertexIDFrom1 = vertexFrom1._id;
        var vertexTo1 = db["firstTo"].save({});
        var vertexIDTo1 = vertexTo1._id;

        var edge1 = db["firstEdge"].save(vertexIDTo1, vertexIDFrom1, {_key: "1"});
        var edgeID1 = edge1._id;

        // G2
        var g2 = graph._create("secondGraph",
          graph._edgeDefinitions(
            graph._relation("secondEdge", ["secondFrom"], ["firstTo"])
          )
        );
        var vertexFrom2 = db["secondFrom"].save({});
        var vertexIDFrom2 = vertexFrom2._id;

        // create edge from G2 to G1 using HTTP API
        const edgeDef = {
          _from: vertexIDFrom2,
          _to: vertexIDTo1
        };

        // create edge pointing from g2 to g1 (edge)
        let req = arango.POST(url + '/' + 'secondGraph' + '/edge/secondEdge', edgeDef);
        expect(req.code).to.equal(202);
        let toBeRemovedEdgeID = req.edge._id;

        // now delete the target edge of g1 (a vertex)
        let req2 = arango.DELETE(url + '/' + 'firstGraph' + '/vertex/' + vertexIDTo1);
        expect(req2.code).to.equal(202);

        var deleted = false;
        try {
          let deletedEdge = db._document(toBeRemovedEdgeID);
        } catch (e) {
          expect(e.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
          deleted = true;
        }

        expect(deleted).to.equal(true);
      });

      it('should not create multiple orphan collection entries', () => {
        var graph = require("@arangodb/general-graph");
        // create an empty graph
        var gName = "firstGraph";
        var collection = "firstEdge";
        var from = "firstTo";
        var to = "firstTo";
        var g1 = graph._create("firstGraph");
        // create a simple edge definition
        const edgeDef = {
          collection: collection,
          from: [from],
          to: [to]
        };
        var createAndDropEdgeDefinition = function () {
          let req = arango.POST(url + '/' + gName + '/edge', edgeDef);
          expect(req.code).to.equal(202);
          // now delete the created edge definition
          let req2 = arango.DELETE(url + '/' + gName + '/edge/' + collection + '?dropCollections=true');
          expect(req2.code).to.equal(202);
        };
        createAndDropEdgeDefinition();
        createAndDropEdgeDefinition();
        createAndDropEdgeDefinition();
        g1 = graph._graph(gName);
        expect(g1.__orphanCollections).to.deep.equal([from]);
        expect(g1.__orphanCollections.length).to.equal(1);
        graph._drop(gName, true);
      });
    });
  });

  describe('graph list extended properties', () => {
    afterEach(() => {
      // Applies only to tests in this describe block
      try {
        gM._drop(graphName, true);
      } catch (ignore) {
      }
      try {
        gM._drop(graphName2, true);
      } catch (ignore) {
      }
    });

    // basic graph constructs
    const firstEdgeDef = [{
      collection: 'firstEdge',
      from: ['firstFrom'],
      to: ['firstTo']
    }];
    const secondEdgeDef = [{
      collection: 'secondEdge',
      from: ['secondFrom'],
      to: ['secondTo']
    }];
    const noOrphans = [];
    const smartOptions = {
      isSmart: true,
      smartGraphAttribute: "Jansen>Sabitzer"
    };
    const smartDisjointOptions = {
      isSmart: true,
      isDisjoint: true,
      smartGraphAttribute: "Jansen>Sabitzer"
    };
    const satelliteOptions = {
      replicationFactor: "satellite"
    };

    // basic validation methods
    const validateBasicGraphsResponse = (res) => {
      expect(res.code).to.equal(200);
      expect(res.error).to.be.false;
      expect(res).to.have.keys("error", "code", "graphs");
      expect(res.graphs).to.be.an('array');
    };

    // basic validation methods
    const validateBasicGraphResponse = (res) => {
      expect(res.code).to.equal(200);
      expect(res.error).to.be.false;
      expect(res).to.have.keys("error", "code", "graph");
      expect(res.graph).to.be.an('object');
    };

    const generateSingleUrl = (graphName) => {
      return `${url}/${graphName}`;
    };

    const generateSingleUrlWithDetails = (graphName) => {
      return `${url}/${graphName}?details=true`;
    };

    const generateAllGraphsUrlWithDetails = () => {
      return `${url}?details=true`;
    };

    const generateAllGraphsUrlWithChecksum = () => {
      return `${url}?onlyHash=true`;
    };

    const createGraphWithProperties = (isSmart, isDisjoint, isSatellite, isHybrid) => {
      let toCreateGraphProperties;

      if (isSmart && isDisjoint) {
        toCreateGraphProperties = _.cloneDeep(smartDisjointOptions);
      } else if (isSmart) {
        toCreateGraphProperties = _.cloneDeep(smartOptions);
      } else if (isSatellite) {
        toCreateGraphProperties = _.cloneDeep(satelliteOptions);
      } else {
        toCreateGraphProperties = {}; // use servers defaults
      }

      if (isHybrid) {
        toCreateGraphProperties.satellites = firstEdgeDef[0].from;
      }

      gM._create(graphName, firstEdgeDef, noOrphans, toCreateGraphProperties);
      const res = arango.GET(generateSingleUrlWithDetails(graphName));

      // graphs are under test above, just a simple double-check to be sure
      if (isSmart) {
        expect(res.graph.isSmart).to.be.true;
        expect(res.graph.isSatellite).to.be.false;
        if (isDisjoint) {
          expect(res.graph.isDisjoint).to.be.true;
        } else {
          expect(res.graph.isDisjoint).to.be.false;
        }
      } else if (isSatellite) {
        expect(res.graph.isSatellite).to.be.true;
      }

      return res;
    }

    const verifyChecksumAfterModifications = (isSmart, isDisjoint, isSatellite, isHybrid) => {
      const fetchChecksum = () => {
        const graph = arango.GET(generateSingleUrlWithDetails(graphName)).graph;
        return graph.checksum;
      };
      const res = createGraphWithProperties(isSmart, isDisjoint, isSatellite, isHybrid);

      console.warn(arango.GET(generateSingleUrlWithDetails(graphName)));

      let checksum = fetchChecksum();
      let jsGraph = gM._graph(graphName);

      // add an orphan collection
      jsGraph._addVertexCollection('deprecateMe');
      expect(checksum).to.not.be.equal(fetchChecksum());
      checksum = fetchChecksum();

      // add a new relation
      const aNewRelation = gM._relation("instructors", ["Padawans"], ["Jedis"]);
      jsGraph._extendEdgeDefinitions(aNewRelation);
      expect(checksum).to.not.be.equal(fetchChecksum());
      checksum = fetchChecksum();

      // modify existing relation
      const aModifiedRelation = gM._relation("instructors", ["Padawans"], ["Jedis", "Siths"]);
      jsGraph._editEdgeDefinitions(aModifiedRelation);
      expect(checksum).to.not.be.equal(fetchChecksum());
      checksum = fetchChecksum();

      // delete existing relation
      jsGraph._deleteEdgeDefinition("instructors", true);
      expect(checksum).to.not.be.equal(fetchChecksum());
      checksum = fetchChecksum();

      // drop an orphan collection
      jsGraph._removeVertexCollection('deprecateMe', true);
      expect(checksum).to.not.be.equal(fetchChecksum());
      checksum = fetchChecksum();

      if (isHybrid) {
        // additional check in case we're adding a new relation or orphan to be created as satellite
        // in hybrid case
        const aNewRelation = gM._relation("instructors", ["Padawans"], ["Jedis"]);
        const satellites = ["Jedis"];
        jsGraph._extendEdgeDefinitions(aNewRelation, {satellites: satellites});
        expect(checksum).to.not.be.equal(fetchChecksum());
        checksum = fetchChecksum();
      }
    };

    const recreateGraphMethod = (isSmart, isDisjoint, isSatellite, isHybrid) => {
      const res = createGraphWithProperties(isSmart, isDisjoint, isSatellite, isHybrid);

      // only pick the values we need for creation
      const attributePickList = [
        'name',
        'isDisjoint',
        'smartGraphAttribute',
        'isSatellite',
        'isSmart',
        'writeConcern',
        'minReplicationFactor',
        'replicationFactor',
        'numberOfShards'
      ];
      if (isHybrid) {
        attributePickList.push('satellites');
      }

      const originalCreatedGraphOrphans = res.graph.orphanCollections;
      const originalCreatedGraphEdgeDefinition = [_.pick(
        res.graph.edgeDefinitions[0], ['collection', 'from', 'to']
      )];
      const originalCreatedGraph = _.pick(res.graph, attributePickList);

      // Now drop the graph again.
      gM._drop(graphName, true);

      // Double check that the graph has been dropped
      const check = arango.GET(generateSingleUrlWithDetails(graphName));
      expect(check.code).to.equal(404);

      // Try to re-create the graph from the created properties
      gM._create(
        originalCreatedGraph.name,
        originalCreatedGraphEdgeDefinition,
        originalCreatedGraphOrphans,
        originalCreatedGraph
      );

      // compare original and newly created graph
      const newly = arango.GET(generateSingleUrlWithDetails(graphName)).graph;
      const newlyOrphans = newly.orphanCollections;
      const newlyEdgeDefinition = [_.pick(
        newly.edgeDefinitions[0], ['collection', 'from', 'to']
      )];
      const newlyCreatedGraph = _.pick(newly, attributePickList);

      expect(newlyOrphans).to.eql(originalCreatedGraphOrphans); // check orphans
      expect(newlyEdgeDefinition).to.eql(originalCreatedGraphEdgeDefinition); // check edgeDefinition
      expect(newlyCreatedGraph).to.eql(originalCreatedGraph); // check properties
      if (isHybrid) {
        expect(newlyCreatedGraph.satellites).to.be.an('array'); // check satellites
        expect(newlyCreatedGraph.satellites.length).to.equal(1);
      }
    };

    /* Satellite exposure tests */
    it('Community Graph - do not expose satellites', () => {
      gM._create(graphName, firstEdgeDef);
      const res = arango.GET(generateSingleUrl(graphName));
      validateBasicGraphResponse(res);
      validateGraphFormat(res.graph);
    });

    if (isEnterprise) {
      it('SmartGraph - do not expose satellites', () => {
        gM._create(graphName, firstEdgeDef, noOrphans, smartOptions);
        const res = arango.GET(generateSingleUrl(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {isSmart: true});
      });

      it('Disjoint SmartGraph - do not expose satellites', () => {
        gM._create(graphName, firstEdgeDef, noOrphans, smartDisjointOptions);
        const res = arango.GET(generateSingleUrl(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {isSmart: true, isDisjoint: true});
      });

      it('SatelliteGraph - do not expose satellites', () => {
        gM._create(graphName, firstEdgeDef, noOrphans, satelliteOptions);
        const res = arango.GET(generateSingleUrl(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {isSatellite: true});
      });

      it('SmartGraph - expose empty satellites', () => {
        gM._create(graphName, firstEdgeDef, noOrphans, smartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {isSmart: true, hasDetails: true});
      });

      it('Disjoint SmartGraph - expose empty satellites', () => {
        gM._create(graphName, firstEdgeDef, noOrphans, smartDisjointOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {isSmart: true, hasDetails: true});
      });

      it('Hybrid SmartGraph - expose satellites (from vertex)', () => {
        let hybridSmartOptions = _.cloneDeep(smartOptions);
        hybridSmartOptions.satellites = firstEdgeDef[0].from;

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites
        });
      });

      it('Hybrid SmartGraph - expose satellites (to vertex)', () => {
        let hybridSmartOptions = _.cloneDeep(smartOptions);
        hybridSmartOptions.satellites = firstEdgeDef[0].to;

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites
        });
      });

      it('Hybrid SmartGraph - expose satellites (from && to vertices)', () => {
        let hybridSmartOptions = _.cloneDeep(smartOptions);
        hybridSmartOptions.satellites = [];
        hybridSmartOptions.satellites.push(firstEdgeDef[0].from[0]);
        hybridSmartOptions.satellites.push(firstEdgeDef[0].to[0]);

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites,
          onlySatellitesCreated: true
        });
      });

      it('Hybrid Disjoint SmartGraph - expose satellites (from vertex)', () => {
        let hybridSmartOptions = _.cloneDeep(smartDisjointOptions);
        hybridSmartOptions.satellites = firstEdgeDef[0].from;

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          isDisjoint: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites
        });
      });

      it('Hybrid Disjoint SmartGraph - expose satellites (to vertex)', () => {
        let hybridSmartOptions = _.cloneDeep(smartDisjointOptions);
        hybridSmartOptions.satellites = firstEdgeDef[0].to;

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          isDisjoint: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites
        });
      });

      it('Hybrid Disjoint SmartGraph - expose satellites (from && to vertices)', () => {
        let hybridSmartOptions = _.cloneDeep(smartDisjointOptions);
        hybridSmartOptions.satellites = [];
        hybridSmartOptions.satellites.push(firstEdgeDef[0].from[0]);
        hybridSmartOptions.satellites.push(firstEdgeDef[0].to[0]);

        gM._create(graphName, firstEdgeDef, noOrphans, hybridSmartOptions);
        const res = arango.GET(generateSingleUrlWithDetails(graphName));
        validateBasicGraphResponse(res);
        validateGraphFormat(res.graph, {
          isSmart: true,
          isDisjoint: true,
          hasDetails: true,
          hybridCollections: hybridSmartOptions.satellites,
          onlySatellitesCreated: true
        });
      });
    }

    /* Checksum tests */
    it('graphs, return only checksum', () => {
      let createOptions = {};
      if (isEnterprise) {
        createOptions = smartOptions;
      }
      gM._create(graphName, firstEdgeDef, noOrphans, createOptions);
      const res = arango.GET(generateAllGraphsUrlWithChecksum());
      expect(res.code).to.equal(200);
      expect(res.error).to.be.false;
      expect(res).to.have.keys("error", "code", "checksum");
      expect(res.checksum).to.be.string;
      expect(res.checksum.length).to.be.greaterThan(1);
    });

    it('graphs, should revoke checksum and details call (not allowed)', () => {
      let createOptions = {};
      if (isEnterprise) {
        createOptions = smartOptions;
      }
      gM._create(graphName, firstEdgeDef, noOrphans, createOptions);
      const res = arango.GET(`${url}?onlyHash=true&details=true`);
      expect(res).to.have.keys("error", "code", "errorMessage", "errorNum");
      expect(res.code).to.equal(400);
      expect(res.error).to.be.true;
      expect(res.errorNum).to.equal(1936);
    });

    it('different graphs, return details including all checksums', () => {
      // means every EdgeDefinition has md5 checksum and every graph has md5 checksum
      let createOptions = {};
      if (isEnterprise) {
        createOptions = smartOptions;
      }
      gM._create(graphName, firstEdgeDef, noOrphans, createOptions);
      gM._create(graphName2, secondEdgeDef, noOrphans, createOptions);

      const res = arango.GET(generateAllGraphsUrlWithDetails());
      validateBasicGraphsResponse(res);

      expect(res.graphs.length).to.equal(2);
      res.graphs.forEach((graph) => {
        if (isEnterprise) {
          validateGraphFormat(graph, {isSmart: true, hasDetails: true});
        } else {
          validateGraphFormat(graph, {isSmart: false, hasDetails: true});
        }
      });
    });

    it('different graphs, overlapping edge definition, return details including all checksums', () => {
      // same edge definitions used in different graphs needs to have the same md5 checksum
      let createOptions = {};
      if (isEnterprise) {
        createOptions = satelliteOptions;
      }
      gM._create(graphName, firstEdgeDef, noOrphans, createOptions);
      gM._create(graphName2, firstEdgeDef, noOrphans, createOptions);

      const res = arango.GET(generateAllGraphsUrlWithDetails());

      validateBasicGraphsResponse(res);
      expect(res.graphs.length).to.equal(2);
      res.graphs.forEach((graph) => {
        if (isEnterprise) {
          validateGraphFormat(graph, {isSatellite: true, hasDetails: true});
        } else {
          validateGraphFormat(graph, {isSatellite: false, hasDetails: true});
        }
      });

      // global checksum must differ
      const checksumGraph = res.graphs[0].checksum;
      const checksumGraph2 = res.graphs[1].checksum;
      expect(checksumGraph).to.not.equal(checksumGraph2);

      // edge definition checksum must be equal
      const checksumEdgeDefinition = res.graphs[0].edgeDefinitions[0].checksum;
      const checksumEdgeDefinition2 = res.graphs[1].edgeDefinitions[0].checksum;
      expect(checksumEdgeDefinition).to.be.equal(checksumEdgeDefinition2);
    });

    it('different graphs, non overlapping edge definition, return details including all checksums', () => {
      // different edge definitions used in different graphs needs to have different md5 checksums
      let createOptions = {};
      if (isEnterprise) {
        createOptions = smartOptions;
      }

      gM._create(graphName, firstEdgeDef, noOrphans, createOptions);
      gM._create(graphName2, secondEdgeDef, noOrphans, createOptions);

      const res = arango.GET(generateAllGraphsUrlWithDetails());

      validateBasicGraphsResponse(res);
      expect(res.graphs.length).to.equal(2);
      res.graphs.forEach((graph) => {
        if (isEnterprise) {
          validateGraphFormat(graph, {isSmart: true, hasDetails: true});
        } else {
          validateGraphFormat(graph, {isSmart: false, hasDetails: true});
        }
      });

      // global checksum must differ
      const checksumGraph = res.graphs[0].checksum;
      const checksumGraph2 = res.graphs[1].checksum;
      expect(checksumGraph).to.not.equal(checksumGraph2);

      // edge definition checksum must be equal
      const checksumEdgeDefinition = res.graphs[0].edgeDefinitions[0].checksum;
      const checksumEdgeDefinition2 = res.graphs[1].edgeDefinitions[0].checksum;
      expect(checksumEdgeDefinition).to.not.be.equal(checksumEdgeDefinition2);
    });

    it('GeneralGraph - modifying a graph should led to a md5 checksum change', () => {
      verifyChecksumAfterModifications(false, false, false, false);
    });

    if (isEnterprise) {
      it('SmartGraph - modifying a graph should led to a md5 checksum change', () => {
        verifyChecksumAfterModifications(true, false, false, false);
      });

      it('Disjoint SmartGraph - modifying a graph should led to a md5 checksum change', () => {
        verifyChecksumAfterModifications(true, true, false, false);
      });

      it('SatelliteGraph - modifying a graph should led to a md5 checksum change', () => {
        verifyChecksumAfterModifications(false, false, true, false);
      });

      it('HybridGraph - modifying a graph should led to a md5 checksum change', () => {
        verifyChecksumAfterModifications(true, false, false, true);
      });

      it('HybridDisjointGraph - modifying a graph should led to a md5 checksum change', () => {
        verifyChecksumAfterModifications(true, true, false, true);
      });
    }

    /* Re-Creation tests */
    if (isEnterprise) {
      it('create SmartGraph, export properties, delete graph and re-create using fetched properties', () => {
        recreateGraphMethod(true, false, false, false);
      });

      it('create Disjoint SmartGraph, export properties, delete graph and re-create using fetched properties', () => {
        recreateGraphMethod(true, true, false, false);
      });

      it('create Satellite SmartGraph, export properties, delete graph and re-create using fetched properties', () => {
        recreateGraphMethod(false, false, true, false);
      });

      it('create Hybrid SmartGraph, export properties, delete graph and re-create using fetched properties', () => {
        recreateGraphMethod(true, false, false, true);
      });

      it('create Hybrid Disjoint SmartGraph, export properties, delete graph and re-create using fetched properties', () => {
        recreateGraphMethod(true, true, false, true);
      });
    }

  });
});
