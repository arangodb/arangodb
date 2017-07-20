'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
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
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const joi = require('joi');
const dd = require('dedent');
const statuses = require('statuses');
const httperr = require('http-errors');
const errors = require('@arangodb').errors;
const cluster = require('@arangodb/cluster');
const Graph = require('@arangodb/general-graph');
const createRouter = require('@arangodb/foxx/router');
const actions = require('@arangodb/actions');
const isEnterprise = require('internal').isEnterprise();

let SmartGraph = {};
if (isEnterprise) {
  SmartGraph = require('@arangodb/smart-graph');
}

const NOT_MODIFIED = statuses('not modified');
const ACCEPTED = statuses('accepted');
const CREATED = statuses('created');
const OK = statuses('ok');

const router = createRouter();
module.context.use(router);

const loadGraph = (name) => {
  try {
    if (isEnterprise) {
      return SmartGraph._graph(name);
    } else {
      return Graph._graph(name);
    }
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_GRAPH_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
};

router.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e.isArangoError) {
      const status = actions.arangoErrorToHttpCode(e.errorNum);
      res.throw(status, e.errorMessage, {errorNum: e.errorNum, cause: e});
    }
    if (e.statusCode === NOT_MODIFIED) {
      res.status(NOT_MODIFIED);
      return;
    }
    throw e;
  }
});

function collectionRepresentation (collection, showProperties, showCount, showFigures) {
  const result = {
    id: collection._id,
    name: collection.name(),
    isSystem: (result.name.charAt(0) === '_'),
    status: collection.status(),
    type: collection.type()
  };

  if (showProperties) {
    const properties = collection.properties();
    result.doCompact = properties.doCompact;
    result.isVolatile = properties.isVolatile;
    result.journalSize = properties.journalSize;
    result.keyOptions = properties.keyOptions;
    result.waitForSync = properties.waitForSync;
    if (cluster.isCoordinator()) {
      result.shardKeys = properties.shardKeys;
      result.numberOfShards = properties.numberOfShards;
    }
  }

  if (showCount) {
    result.count = collection.count();
  }

  if (showFigures) {
    const figures = collection.figures();

    if (figures) {
      result.figures = figures;
    }
  }

  return result;
}

function checkCollection (g, collection) {
  if (!g[collection]) {
    throw Object.assign(
      new httperr.NotFound(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message),
      {errorNum: errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code}
    );
  }
}

function setResponse (res, name, body, code) {
  res.status(code);
  if (body._rev) {
    res.set('etag', body._rev);
  }
  res.json({
    error: false,
    [name]: body,
    code
  });
}

function matchVertexRevision (req, rev) {
  if (req.headers['if-none-match']) {
    if (rev === req.headers['if-none-match'].replace(/(^["']|["']$)/g, '')) {
      throw httperr(NOT_MODIFIED);
    }
  }

  if (req.headers['if-match']) {
    if (rev !== req.headers['if-match'].replace(/(^["']|["']$)/g, '')) {
      throw Object.assign(
        new httperr.PreconditionFailed('wrong revision'),
        {errorNum: errors.ERROR_GRAPH_INVALID_VERTEX.code}
      );
    }
  }

  if (req.queryParams.rev) {
    if (rev !== req.queryParams.rev) {
      throw Object.assign(
        new httperr.PreconditionFailed('wrong revision'),
        {errorNum: errors.ERROR_GRAPH_INVALID_VERTEX.code}
      );
    }
  }
}

function graphForClient (g) {
  return {
    name: g.__name,
    edgeDefinitions: g.__edgeDefinitions,
    orphanCollections: g._orphanCollections(),
    isSmart: g.__isSmart || false,
    numberOfShards: g.__numberOfShards || 0,
    smartGraphAttribute: g.__smartGraphAttribute || '',
    _id: g.__id,
    _rev: g.__rev
  };
}

// PHP clients like to convert booleans to numbers
const phpCompatFlag = joi.alternatives().try(
  joi.boolean(),
  joi.number().integer()
);

const graphName = joi.string()
  .description('Name of the graph.');

const vertexCollectionName = joi.string()
  .description('Name of the vertex collection.');

const edgeCollectionName = joi.string()
  .description('Name of the edge collection.');

const dropCollectionFlag = phpCompatFlag
  .description('Flag to drop collection as well.');

const definitionEdgeCollectionName = joi.string()
  .description('Name of the edge collection in the definition.');

const waitForSyncFlag = phpCompatFlag
  .description('define if the request should wait until synced to disk.');

const vertexKey = joi.string()
  .description('_key attribute of one specific vertex');

const edgeKey = joi.string()
  .description('_key attribute of one specific edge.');

const keepNullFlag = phpCompatFlag
  .description('define if null values should not be deleted.').default(true);

// Graph Creation

router.get('/', function (req, res) {
  setResponse(res, 'graphs', Graph._listObjects(), OK);
})
  .summary('List graphs')
  .description('Creates a list of all available graphs.');

router.post('/', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  let g;
  try {
    if (isEnterprise && req.body.isSmart === true) {
      const smartGraphAttribute = req.body.options.smartGraphAttribute;
      const numberOfShards = req.body.options.numberOfShards;
      g = SmartGraph._create(
        req.body.name,
        req.body.edgeDefinitions,
        req.body.orphanCollections,
        {waitForSync, numberOfShards, smartGraphAttribute}
      );
    } else {
      if (req.body.options && req.body.options.numberOfShards && cluster.isCluster()) {
        const numberOfShards = req.body.options.numberOfShards || 1;
        const replicationFactor = req.body.options.replicationFactor || 1;
        g = Graph._create(
          req.body.name,
          req.body.edgeDefinitions,
          req.body.orphanCollections,
          {waitForSync, numberOfShards, replicationFactor}
        );
      } else if (req.body.options && req.body.options.replicationFactor && cluster.isCluster()) {
        const numberOfShards = req.body.options.numberOfShards || 1;
        const replicationFactor = req.body.options.replicationFactor || 1;
        g = Graph._create(
          req.body.name,
          req.body.edgeDefinitions,
          req.body.orphanCollections,
          {waitForSync, numberOfShards, replicationFactor}
        );
      } else {
        g = Graph._create(
          req.body.name,
          req.body.edgeDefinitions,
          req.body.orphanCollections,
          {waitForSync}
        );
      }
    }
  } catch (e) {
    if (e.isArangoError) {
      switch (e.errorNum) {
        case errors.ERROR_BAD_PARAMETER.code:
        case errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code:
        case errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.code:
        case errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.code:
        case errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code:
        case errors.ERROR_ARANGO_DUPLICATE_NAME.code:
          require('console').error("Message:", e.errorMessage);
          throw Object.assign(
            new httperr.BadRequest(e.errorMessage),
            {errorNum: e.errorNum, cause: e}
          );

        case errors.ERROR_GRAPH_DUPLICATE.code:
          throw Object.assign(
            new httperr.Conflict(e.errorMessage),
            {errorNum: e.errorNum, cause: e}
          );
        default:
      }
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), waitForSync ? CREATED : ACCEPTED);
})
  .queryParam('waitForSync', waitForSyncFlag)
  .body(joi.object({
    name: joi.string().required(),
    edgeDefinitions: joi.array().optional(),
    orphanCollections: joi.array().optional(),
    isSmart: joi.boolean().optional(),
    options: joi.object({
      smartGraphAttribute: joi.string().optional(),
      replicationFactor: joi.number().integer().greater(0).optional(),
      numberOfShards: joi.number().integer().greater(0).optional()
    }).optional()
  }).required(), 'The required information for a graph')
  .error('bad request', 'Graph creation error.')
  .error('conflict', 'Graph creation error.')
  .summary('Creates a new graph')
  .description('Creates a new graph object');

router.get('/:graph', function (req, res) {
  const name = req.pathParams.graph;
  const g = loadGraph(name);
  setResponse(res, 'graph', graphForClient(g), OK);
})
  .pathParam('graph', graphName)
  .error('not found', 'Graph could not be found.')
  .summary('Get information of a graph')
  .description(dd`
  Selects information for a given graph.
  Will return the edge definitions as well as the vertex collections.
  Or throws a 404 if the graph does not exist.
`);

router.delete('/:graph', function (req, res) {
  const dropCollections = Boolean(req.queryParams.dropCollections);
  const name = req.pathParams.graph;
  try {
    Graph._drop(name, dropCollections);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_GRAPH_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'removed', true, ACCEPTED);
})
  .pathParam('graph', graphName)
  .queryParam('dropCollections', dropCollectionFlag)
  .error('not found', 'The graph does not exist.')
  .summary('Drops an existing graph')
  .description(dd`
  Drops an existing graph object by name.
  Optionally all collections not used by other graphs
  can be dropped as well.
`);

// Definitions

router.get('/:graph/vertex', function (req, res) {
  const name = req.pathParams.graph;
  const excludeOrphans = req.queryParams.excludeOrphans;
  const g = loadGraph(name);
  const mapFunc = (
  req.pathParams.collectionObjects
    ? (c) => collectionRepresentation(c, false, false, false)
    : (c) => c.name()
  );
  setResponse(res, 'collections', _.map(g._vertexCollections(excludeOrphans), mapFunc).sort(), OK);
})
  .pathParam('graph', graphName)
  .error('not found', 'The graph could not be found.')
  .summary('List all vertex collections.')
  .description('Gets the list of all vertex collections.');

router.post('/:graph/vertex', function (req, res) {
  const name = req.pathParams.graph;
  const g = loadGraph(name);
  try {
    g._addVertexCollection(req.body.collection);
  } catch (e) {
    if (e.isArangoError) {
      switch (e.errorNum) {
        case errors.ERROR_BAD_PARAMETER.code:
        case errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.code:
        case errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.code:
        case errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code:
          throw Object.assign(
              new httperr.BadRequest(e.errorMessage),
              {errorNum: e.errorNum, cause: e});

        case errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code:
          throw Object.assign(
              new httperr.NotFound(e.errorMessage),
              {errorNum: e.errorNum, cause: e});
      }
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), ACCEPTED);
})
  .pathParam('graph', graphName)
  .body(joi.object({
    collection: joi.any().required()
  }).required(), 'The vertex collection to be stored.')
  .error('bad request', 'The vertex collection is invalid.')
  .error('not found', 'The graph could not be found.')
  .summary('Create a new vertex collection.')
  .description('Stores a new vertex collection. This has to contain the vertex-collection name.');

router.delete('/:graph/vertex/:collection', function (req, res) {
  const dropCollection = Boolean(req.queryParams.dropCollection);
  const name = req.pathParams.graph;
  const defName = req.pathParams.collection;
  const g = loadGraph(name);
  try {
    g._removeVertexCollection(defName, dropCollection);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    if (e.isArangoError && e.errorNum === errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.code) {
      throw Object.assign(
        new httperr.BadRequest(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .queryParam('dropCollection', dropCollectionFlag)
  .error('bad request', 'The collection is not found or part of an edge definition.')
  .error('not found', 'The graph could not be found.')
  .summary('Delete a vertex collection.')
  .description('Removes a vertex collection from this graph. If this collection is used in one or more edge definitions');

router.get('/:graph/edge', function (req, res) {
  const name = req.pathParams.graph;
  const g = loadGraph(name);
  setResponse(res, 'collections', _.map(g._edgeCollections(), (c) => c.name()).sort(), OK);
})
  .pathParam('graph', graphName)
  .error('not found', 'The graph could not be found.')
  .summary('List all edge collections.')
  .description('Get the list of all edge collection.');

router.post('/:graph/edge', function (req, res) {
  const name = req.pathParams.graph;
  const g = loadGraph(name);
  try {
    g._extendEdgeDefinitions(req.body);
  } catch (e) {
    if (e.isArangoError) {
      switch (e.errorNum) {
        case errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code:
        case errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code:
        case errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code:
          throw Object.assign(
            new httperr.BadRequest(e.errorMessage),
            {errorNum: e.errorNum, cause: e}
          );
      }
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), ACCEPTED);
})
  .pathParam('graph', graphName)
  .body(joi.any().required(), 'The edge definition to be stored.')
  .error('bad request', 'The edge definition is invalid.')
  .error('not found', 'The graph could not be found.')
  .summary('Create a new edge definition.')
  .description(dd`
  Stores a new edge definition with the information contained within the body.
  This has to contain the edge-collection name,
  as well as set of from and to collections-names respectively.
`);

router.put('/:graph/edge/:definition', function (req, res) {
  const name = req.pathParams.graph;
  const defName = req.pathParams.definition;
  const g = loadGraph(name);
  if (defName !== req.body.collection) {
    throw Object.assign(
      new httperr.NotFound(errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message),
      {errorNum: errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code}
    );
  }
  try {
    g._editEdgeDefinitions(req.body);
  } catch (e) {
    if (e.isArangoError) {
      switch (e.errorNum) {
        case errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code:
        case errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code:
          throw Object.assign(
            new httperr.BadRequest(e.errorMessage),
            {errorNum: e.errorNum, cause: e}
          );
      }
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('definition', definitionEdgeCollectionName)
  .body(joi.object().required(), 'The edge definition to be stored.')
  .error('bad request', 'The edge definition is invalid.')
  .error('not found', 'The graph could not be found.')
  .summary('Replace an edge definition.')
  .description(dd`
  Replaces an existing edge definition with the information contained within the body.
  This has to contain the edge-collection name,
  as well as set of from and to collections-names respectively.
  This will also change the edge definitions of all other graphs using this definition as well.
`);

router.delete('/:graph/edge/:definition', function (req, res) {
  const dropCollection = Boolean(req.queryParams.dropCollection);
  const name = req.pathParams.graph;
  const defName = req.pathParams.definition;
  const g = loadGraph(name);
  try {
    g._deleteEdgeDefinition(defName, dropCollection);
  } catch (e) {
    if (e.isArangoError &&
        errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code === e.errorNum) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'graph', graphForClient(g), ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('definition', definitionEdgeCollectionName)
  .queryParam('dropCollection', dropCollectionFlag)
  .error('not found', 'The graph could not be found.')
  .summary('Delete an edge definition.')
  .description(dd`
  Removes an existing edge definition from this graph.
  All data stored in the edge collection are dropped as well
  as long as it is not used in other graphs.
`);

// Vertex Operations

router.post('/:graph/vertex/:collection', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let meta;
  try {
    meta = g[collection].save(req.body, {waitForSync});
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_GRAPH_INVALID_EDGE.code) {
      throw Object.assign(
        new httperr.BadRequest(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'vertex', meta, waitForSync ? CREATED : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .queryParam('waitForSync', waitForSyncFlag)
  .body(joi.any().required(), 'The document to be stored')
  .error('bad request', 'The edge definition is invalid.')
  .error('not found', 'Graph or collection not found.')
  .summary('Create a new vertex.')
  .description('Stores a new vertex with the information contained within the body into the given collection.');

router.get('/:graph/vertex/:collection/:key', function (req, res) {
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  setResponse(res, 'vertex', doc, OK);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .pathParam('key', vertexKey)
  .error('not found', 'The vertex does not exist.')
  .summary('Get a vertex.')
  .description('Gets a vertex with the given key if it is contained within your graph.');

router.put('/:graph/vertex/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  const meta = g[collection].replace(id, req.body, {waitForSync});
  setResponse(res, 'vertex', meta, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .pathParam('key', vertexKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .body(joi.any().required(), 'The document to be stored')
  .error('bad request', 'The vertex is invalid.')
  .error('not found', 'The vertex does not exist.')
  .summary('Replace a vertex.')
  .description(dd`
  Replaces a vertex with the given id by the content in the body.
  This will only run successfully if the vertex is contained within the graph.
`);

router.patch('/:graph/vertex/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const keepNull = Boolean(req.queryParams.keepNull);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  const meta = g[collection].update(id, req.body, {waitForSync, keepNull});
  setResponse(res, 'vertex', meta, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .pathParam('key', vertexKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .queryParam('keepNull', keepNullFlag)
  .body(joi.any().required(), 'The values that should be modified')
  .error('bad request', 'The vertex is invalid.')
  .error('not found', 'The vertex does not exist.')
  .summary('Update a vertex.')
  .description(dd`
  Updates a vertex with the given id by adding the content in the body.
  This will only run successfully if the vertex is contained within the graph.
`);

router.delete('/:graph/vertex/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  let didRemove;
  try {
    didRemove = g[collection].remove(id, {waitForSync});
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'removed', didRemove, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', vertexCollectionName)
  .pathParam('key', vertexKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .error('not found', 'The vertex does not exist.')
  .summary('Delete a vertex.')
  .description(dd`
  Deletes a vertex with the given id, if it is contained within the graph.
  Furthermore all edges connected to this vertex will be deleted.
`);

// Edge Operations

router.post('/:graph/edge/:collection', function (req, res) {
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  if (!req.body._from || !req.body._to) {
    throw Object.assign(
      new httperr.Gone(errors.ERROR_GRAPH_INVALID_EDGE.message),
      {errorNum: errors.ERROR_GRAPH_INVALID_EDGE.code}
    );
  }
  const g = loadGraph(name);
  checkCollection(g, collection);
  let meta;
  try {
    meta = g[collection].save(req.body);
  } catch (e) {
    if (e.errorNum !== errors.ERROR_GRAPH_INVALID_EDGE.code) {
      throw Object.assign(
        new httperr.Gone(e.errorMessage),
        {errorNum: errors.ERROR_GRAPH_INVALID_EDGE.code, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'edge', meta, ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', edgeCollectionName)
  .body(joi.object().required(), 'The edge to be stored. Has to contain _from and _to attributes.')
  .error('bad request', 'The edge is invalid.')
  .error('not found', 'Graph or collection not found.')
  .summary('Create a new edge.')
  .description('Stores a new edge with the information contained within the body into the given collection.');

router.get('/:graph/edge/:collection/:key', function (req, res) {
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  setResponse(res, 'edge', doc, OK);
})
  .pathParam('graph', graphName)
  .pathParam('collection', edgeCollectionName)
  .pathParam('key', edgeKey)
  .error('not found', 'The edge does not exist.')
  .summary('Load an edge.')
  .description('Loads an edge with the given id if it is contained within your graph.');

router.put('/:graph/edge/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  const meta = g[collection].replace(id, req.body, {waitForSync});
  setResponse(res, 'edge', meta, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', edgeCollectionName)
  .pathParam('key', edgeKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .body(joi.any().required(), 'The document to be stored. _from and _to attributes are ignored')
  .error('bad request', 'The edge is invalid.')
  .error('not found', 'The edge does not exist.')
  .summary('Replace an edge.')
  .description(dd`
  Replaces an edge with the given id by the content in the body.
  This will only run successfully if the edge is contained within the graph.
`);

router.patch('/:graph/edge/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const keepNull = Boolean(req.queryParams.keepNull);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  const meta = g[collection].update(id, req.body, {waitForSync, keepNull});
  setResponse(res, 'edge', meta, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', edgeCollectionName)
  .pathParam('key', edgeKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .queryParam('keepNull', keepNullFlag)
  .body(joi.any().required(), 'The values that should be modified. _from and _to attributes are ignored')
  .error('bad request', 'The edge is invalid.')
  .error('not found', 'The edge does not exist.')
  .summary('Update an edge.')
  .description(dd`
  Updates an edge with the given id by adding the content in the body.
  This will only run successfully if the edge is contained within the graph.
`);

router.delete('/:graph/edge/:collection/:key', function (req, res) {
  const waitForSync = Boolean(req.queryParams.waitForSync);
  const name = req.pathParams.graph;
  const collection = req.pathParams.collection;
  const key = req.pathParams.key;
  const id = `${collection}/${key}`;
  const g = loadGraph(name);
  checkCollection(g, collection);
  let doc;
  try {
    doc = g[collection].document(id);
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  matchVertexRevision(req, doc._rev);
  let didRemove;
  try {
    didRemove = g[collection].remove(id, {waitForSync});
  } catch (e) {
    if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw Object.assign(
        new httperr.NotFound(e.errorMessage),
        {errorNum: e.errorNum, cause: e}
      );
    }
    throw e;
  }
  setResponse(res, 'removed', didRemove, waitForSync ? OK : ACCEPTED);
})
  .pathParam('graph', graphName)
  .pathParam('collection', edgeCollectionName)
  .pathParam('key', edgeKey)
  .queryParam('waitForSync', waitForSyncFlag)
  .error('not found', 'The edge does not exist.')
  .summary('Delete an edge.')
  .description('Deletes an edge with the given id, if it is contained within the graph.');
