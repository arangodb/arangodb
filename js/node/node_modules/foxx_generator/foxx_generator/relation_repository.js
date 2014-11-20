(function () {
  'use strict';
  var RelationRepository,
    _ = require('underscore');

  RelationRepository = function (from, to, relation, graph) {
    this.edgeCollectionName = relation.edgeCollectionName;
    this.fromId = function (key) { return from.collectionName + '/' + key; };
    this.toId = function (key) { return to.collectionName + '/' + key; };
    this.graph = graph;
  };

  _.extend(RelationRepository.prototype, {
    replaceRelation: function (sourceKey, destinationKey) {
      var sourceId = this.fromId(sourceKey),
        destinationId = this.toId(destinationKey),
        edgeCollectionName = this.edgeCollectionName,
        graph = this.graph;

      graph.checkIfVerticesExist([destinationId, sourceId]);
      graph.removeEdges({ vertexId: sourceId, edgeCollectionName: edgeCollectionName, throwError: false });
      graph.createEdge({ edgeCollectionName: edgeCollectionName, sourceId: sourceId, destinationId: destinationId });
    },

    addRelations: function (sourceKey, destinationKeys) {
      var sourceId = this.fromId(sourceKey),
        destinationIds = _.map(destinationKeys, this.toId, this),
        edgeCollectionName = this.edgeCollectionName,
        graph = this.graph;

      graph.checkIfVerticesExist(_.union(sourceId, destinationIds));
      _.each(destinationIds, function (destinationId) {
        graph.createEdge({ edgeCollectionName: edgeCollectionName, sourceId: sourceId, destinationId: destinationId });
      });
    },

    deleteRelation: function (sourceKey) {
      var sourceId = this.fromId(sourceKey),
        edgeCollectionName = this.edgeCollectionName,
        graph = this.graph;

      graph.checkIfVerticesExist([sourceId]);
      graph.removeEdges({ vertexId: sourceId, edgeCollectionName: edgeCollectionName });
    }
  });

  exports.RelationRepository = RelationRepository;
}());
