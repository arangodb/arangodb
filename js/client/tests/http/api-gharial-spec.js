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

});
