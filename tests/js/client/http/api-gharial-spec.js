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

const chai = require('chai');
const expect = chai.expect;
chai.Assertion.addProperty('does', function () { return this; });

const arangodb = require('@arangodb');
const request = require('@arangodb/request');

const ERRORS = arangodb.errors;
const db = arangodb.db;
const internal = require('internal');
const wait = internal.wait;
const url = '/_api/gharial';

describe('_api/gharial', () => {

  const graphName = 'UnitTestGraph';
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
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

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
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

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
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - invalid from', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'peter',
      _to: 'persons/charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - invalid to', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'persons/peter',
      _to: 'charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - invalid from and to attributes', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
      _from: 'peter',
      _to: 'charlie'
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - invalid _to attribute', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - _to vertex collection not part of graph definition', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - _from vertex collection not part of graph definition', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - not existing _to document', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - invalid _from attribute', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - not existing _from document', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT update (PATCH) - not existing _from && _to document', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    let req = request.patch(url + '/' + exampleGraphName + '/edge/knows/' + _key, {
      body: JSON.stringify(edgeDef)
    });
    print(req);
    expect(req.statusCode).to.equal(404);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT replace (PUT) - not existing _to document', () => {
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT replace (PUT) - not existing _from document', () => {
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT replace (PUT) - not existing _from && _to document', () => {
  });

  it('should check if edges can only be created if their _from and _to vertices are existent - should NOT create - missing from and to attributes', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
    expect(db._collection(eName)).to.be.null; // edgec
    expect(db._collection(vName)).to.be.null; // vertexc
    const g = examples.loadGraph(exampleGraphName);
    expect(g).to.not.be.null;

    const edgeDef = {
    };
    let req = request.post(url + '/' + exampleGraphName + '/edge/knows', {
      body: JSON.stringify(edgeDef)
    });
    expect(req.statusCode).to.equal(400);
    expect(req.json.errorNum).to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);

    expect(db._collection(eName)).to.not.be.null;
    expect(db._collection(vName)).to.not.be.null;
  });

  it('should check if incident edges are deleted with a vertex', () => {
    const examples = require('@arangodb/graph-examples/example-graph');
    const exampleGraphName = 'knows_graph';
    const vName = 'persons';
    const eName = 'knows';
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
    const res = request.delete(
      `${url}/${exampleGraphName}/vertex/${vName}/${bob}`
    );

    // check response
    expect(res).to.be.an.instanceof(request.Response);
    expect(res.body).to.be.a('string');
    const body = JSON.parse(res.body);
    // 202 without waitForSync (default)
    expect(body).to.eql({
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
    const res = request.delete(
      `${url}/${exampleGraphName}/vertex/${vName}/${bob}`
    );

    // check response
    expect(res).to.be.an.instanceof(request.Response);
    expect(res.body).to.be.a('string');
    const body = JSON.parse(res.body);
    // 202 without waitForSync (default)
    expect(body).to.eql({
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

    const res = request.put(
      `${url}/${exampleGraphName}/edge/${eName}/${e._key}`,
      {body: JSON.stringify(newEdge)}
    );

    // 202 without waitForSync (default)
    expect(res.statusCode).to.equal(202);
    expect(res.json.code).to.equal(202);
    expect(res.json.error).to.equal(false);
    expect(res.json.edge._key).to.equal(e._key);

    expect(db.knows.document(e._key))
      .to.be.an('object')
      .that.has.property('newAttribute')
      .which.equals('new value');
  });

  it('should check that edges can NOT be replaced if their _from or _to' +
    ' attribute is missing', () => {
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

      const res = request.put(
        `${url}/${exampleGraphName}/edge/${eName}/${e._key}`,
        {body: JSON.stringify(newEdge)}
      );

      expect(res.statusCode, description).to.equal(400);
      expect(res.json.errorNum, description)
        .to.equal(ERRORS.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);
    }

    expect(db.knows.document(e._key))
      .to.be.an('object')
      .that.does.not.have.property('new');

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
      expect(req.json.errorMessage).to.equal(ERRORS.ERROR_HTTP_PRECONDITION_FAILED.message);
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
      let req = request.post(url + '/' + gName + '/edge', {
        body: JSON.stringify(edgeDef)
      });
      expect(req.statusCode).to.equal(202);
       // now delete the created edge definition
      let req2 = request.delete(url + '/' + gName + '/edge/' + collection + '?dropCollections=true');
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
