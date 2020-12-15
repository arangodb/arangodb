/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';

  window.Graph = Backbone.Model.extend({
    idAttribute: '_key',

    urlRoot: arangoHelper.databaseUrl('/_api/gharial'),

    isNew: function () {
      return !this.get('_id');
    },

    parse: function (raw) {
      return raw.graph || raw;
    },

    addEdgeDefinition: function (edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: 'POST',
          url: this.urlRoot + '/' + this.get('_key') + '/edge',
          data: JSON.stringify(edgeDefinition),
          error: function (err) {
            arangoHelper.arangoError(err.responseJSON.errorMessage);
          }
        }
      );
    },

    deleteEdgeDefinition: function (edgeCollection) {
      $.ajax(
        {
          async: false,
          type: 'DELETE',
          url: this.urlRoot + '/' + this.get('_key') + '/edge/' + edgeCollection,
          error: function (err) {
            arangoHelper.arangoError(err.responseJSON.errorMessage);
          }
        }
      );
    },

    modifyEdgeDefinition: function (edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: 'PUT',
          url: this.urlRoot + '/' + this.get('_key') + '/edge/' + edgeDefinition.collection,
          data: JSON.stringify(edgeDefinition),
          error: function (err) {
            arangoHelper.arangoError(err.responseJSON.errorMessage);
          }
        }
      );
    },

    addVertexCollection: function (vertexCollectionName) {
      $.ajax(
        {
          async: false,
          type: 'POST',
          url: this.urlRoot + '/' + this.get('_key') + '/vertex',
          data: JSON.stringify({collection: vertexCollectionName}),
          error: function (err) {
            arangoHelper.arangoError(err.responseJSON.errorMessage);
          }
        }
      );
    },

    deleteVertexCollection: function (vertexCollectionName) {
      $.ajax(
        {
          async: false,
          type: 'DELETE',
          url: this.urlRoot + '/' + this.get('_key') + '/vertex/' + vertexCollectionName,
          error: function (err) {
            arangoHelper.arangoError(err.responseJSON.errorMessage);
          }
        }
      );
    },

    defaults: {
      name: '',
      edgeDefinitions: [],
      orphanCollections: []
    }
  });
}());
