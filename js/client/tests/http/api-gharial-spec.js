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
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;

const arangodb = require('@arangodb');
const request = require('@arangodb/request');

const ERRORS = arangodb.errors;
const db = arangodb.db;
const wait = require('internal').wait;
const extend = require('lodash').extend;

describe('_api/gharial', () => {

  const graphName = 'UnitTestGraph';
  const vColName = 'UnitTestVertices';
  const eColName = 'UnitTestRelations';
  const oColName = 'UnitTestOrphans';
  const oColName2 = 'UnitTestOrphansSecond';
  const url = '/_api/gharial';

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
  };

  beforeEach(cleanup);

  afterEach(cleanup);

  it('should create a graph without orphans', () => {
    const graphDef = {
      "name": graphName,
      "edgeDefinitions": [{
              "collection":  eColName,
              "from": [vColName],
              "to": [vColName]
          }
      ],
      "isSmart": false
    };
    expect(db._collection(eColName)).to.be.null;
    expect(db._collection(vColName)).to.be.null;
    let req = request.post(url, {
      body: JSON.stringify(graphDef)
    });
    expect(req.statusCode).to.equal(202);

    // This is all async give it some time
    do {
      wait(0.1);
      req = request.get(url + "/" + graphName); 
    } while (req.statusCode !== 200);

    expect(db._collection(eColName)).to.not.be.null; 
    expect(db._collection(vColName)).to.not.be.null;
  });

  it('should create a graph with orphans', () => {
    const graphDef = {
      "name": graphName,
      "edgeDefinitions": [{
            "collection":  eColName,
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
    let req = request.post(url, {
      body: JSON.stringify(graphDef)
    });
    expect(req.statusCode).to.equal(202);

    // This is all async give it some time
    do {
      wait(0.1);
      req = request.get(url + "/" + graphName);
    } while (req.statusCode !== 200);

    expect(db._collection(eColName)).to.not.be.null;
    expect(db._collection(vColName)).to.not.be.null;
    expect(db._collection(oColName)).to.not.be.null;
    expect(db._collection(oColName2)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should create', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/bob',
      _to: 'persons/charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(202);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing from document', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/notavailable',
      _to: 'persons/charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing to document', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/bob',
      _to: 'persons/notavailable'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing both from and to documents', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/notavailable',
      _to: 'persons/notavailable'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing from collection', () => {
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
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_COLLECTION_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing to collection', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/bob',
      _to: 'xxx/charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_COLLECTION_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing from and to collection', () => {
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
      _to: 'xxx/charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_COLLECTION_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should also remove an edge in a second graph if connection in first graph is removed', () => {
    // G1 = X ---- e ----> Y
    // G2 = A --^               // A points to e in G1
    //      A ---- b ----> B    // A points to B in G2

    // TODO tests failing because of no proper cleanup (angeblich)
    var graph = require("@arangodb/general-graph");
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
    let req = request.post(url + '/' + 'secondGraph' + '/edge/secondEdge', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(202);
    let toBeRemovedEdgeID = req.json.edge._id;

    // now delete the target edge of g1
    let req2 = request.delete(url + '/' + 'firstGraph' + '/edge/' + edgeID1);
    expect(req2.statusCode).to.equal(202);

    var deleted = false;
    try {
      let deletedEdge = db._document(toBeRemovedEdgeID);
    } catch (e) {
      expect(e.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      deleted = true;
    }

    graph._drop("firstGraph", true);
    graph._drop("secondGraph", true);

    expect(deleted).to.equal(true);
  });

  it('should also remove an edge in a second graph if connected vertex in first graph is removed', () => {
    // G1 = X ---- e ----> Y
    // G2 = A -------------^    // A points to Y in G1
    //      A ---- b ----> B    // A points to B in G2

    var graph = require("@arangodb/general-graph");
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
    let req = request.post(url + '/' + 'secondGraph' + '/edge/secondEdge', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(202);
    let toBeRemovedEdgeID = req.json.edge._id;

    // now delete the target edge of g1 (a vertex)
    let req2 = request.delete(url + '/' + 'firstGraph' + '/vertex/' + vertexIDTo1);
    expect(req2.statusCode).to.equal(202);

    var deleted = false;
    try {
      let deletedEdge = db._document(toBeRemovedEdgeID);
    } catch (e) {
      expect(e.errorNum).to.equal(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      deleted = true;
    }

    graph._drop("firstGraph", true);
    graph._drop("secondGraph", true);

    expect(deleted).to.equal(true);
  });

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

    let req = request.get(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key , {
      headers: {
        'if-match': revision
      }
    });
    expect(req.statusCode).to.equal(200);
    expect(req.json.edge).to.deep.equal(doc);
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
      let req = request.get(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key , {
        headers: {
          'if-match': rev
        }
      });
      expect(req.json.error).to.equal(true);
      expect(req.statusCode).to.equal(ERRORS.ERROR_HTTP_PRECONDITION_FAILED.code);
      expect(req.json.code).to.equal(ERRORS.ERROR_HTTP_PRECONDITION_FAILED.code);
      expect(req.json.errorMessage).to.equal('wrong revision');
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

    let req = request.get(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key , {
      headers: {
        'if-none-match': revision
      }
    });
    expect(req.status).to.equal(304);
    expect(req.json).to.equal(undefined);
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
      let req = request.get(url + '/' + exampleGraphName + '/edge/' + vName + '/' + key , {
        headers: {
          'if-none-match': rev
        }
      });
      expect(req.statusCode).to.equal(200);
      expect(req.json.edge).to.deep.equal(doc);
    });
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
      let req = request.post(url + '/' + gName + '/edge', {
        body: JSON.stringify(edgeDef)
      });
      expect(req.statusCode).to.equal(202);

      // now delete the created edge definition
      let req2 = request.delete(url + '/' + gName + '/edge/' + collection + '?dropCollection=true');
      expect(req2.statusCode).to.equal(202);
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
