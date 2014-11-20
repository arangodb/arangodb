(function () {
  'use strict';
  var graphModule = require('org/arangodb/general-graph'),
    ArangoError = require('internal').ArangoError,
    _ = require('underscore'),
    report = require('./reporter').report,
    Graph,
    VertexNotFound = require('./vertex_not_found').VertexNotFound,
    tryAndHandleArangoError,
    alreadyExists;

  tryAndHandleArangoError = function (func, errHandler) {
    try {
      func();
    } catch (e) {
      if (e instanceof ArangoError) {
        errHandler();
      } else {
        throw e;
      }
    }
  };

  alreadyExists = function (type, name) {
    return function () {
      report('%s "%s" already added. Leaving it untouched.', type, name);
    };
  };

  Graph = function (name, appContext) {
    var that = this;
    this.appContext = appContext;

    tryAndHandleArangoError(function () {
      that.graph = graphModule._graph(name);
    }, function () {
      that.graph = graphModule._create(name);
    });
  };

  _.extend(Graph.prototype, {
    extendEdgeDefinitions: function (rawEdgeCollectionName, from, to) {
      var vertexCollections, edgeCollectionName, edgeDefinition, graph;
      edgeCollectionName = this.appContext.collectionName(rawEdgeCollectionName);

      if (from.type === 'entity' && to.type === 'entity' && from.collectionName && to.collectionName) {
        vertexCollections = [ from.collectionName, to.collectionName ];
        edgeDefinition = graphModule._undirectedRelation(edgeCollectionName, vertexCollections);
        graph = this.graph;

        tryAndHandleArangoError(function () {
          graph._extendEdgeDefinitions(edgeDefinition);
        }, alreadyExists('EdgeDefinition', edgeCollectionName));
      } else {
        report('Invalid edge definition for "%s" and "%s"', from.collectionName, to.collectionName);
      }

      return edgeCollectionName;
    },

    addVertexCollection: function (collectionName) {
      var prefixedCollectionName = this.appContext.collectionName(collectionName),
        graph = this.graph;

      tryAndHandleArangoError(function () {
        graph._addVertexCollection(prefixedCollectionName, true);
      }, alreadyExists('Collection', prefixedCollectionName));

      return this.graph[prefixedCollectionName];
    },

    neighbors: function (id, options) {
      return this.graph._neighbors(id, options);
    },

    edges: function (id, options) {
      return this.graph._vertices(id).edges().restrict(options.edgeCollectionRestriction).toArray();
    },

    removeEdges: function (options) {
      var graph = this.graph,
        vertexId = options.vertexId,
        edgeCollectionName = options.edgeCollectionName,
        edges;

      edges = this.edges(vertexId, {
        edgeCollectionRestriction: [edgeCollectionName],
      });

      _.each(edges, function (edge) {
        graph[edgeCollectionName].remove(edge._id);
      });
    },

    checkIfVerticesExist: function (ids) {
      if (!_.every(ids, this.hasVertex, this)) {
        throw new VertexNotFound();
      }
    },

    hasVertex: function (id) {
      return this.graph._vertices(id).count() > 0;
    },

    areConnected: function (sourceId, destinationId) {
      return this.graph._edges([
        { _from: sourceId, _to: destinationId },
        { _from: destinationId, _to: sourceId }
      ]).count() > 0;
    },

    createEdge: function (options) {
      var sourceId = options.sourceId,
        destinationId = options.destinationId,
        edgeCollectionName = options.edgeCollectionName,
        edgeCollection = this.graph[edgeCollectionName];

      if (!this.areConnected(sourceId, destinationId)) {
        edgeCollection.save(sourceId, destinationId, {});
      }
    }
  });

  exports.Graph = Graph;
}());
