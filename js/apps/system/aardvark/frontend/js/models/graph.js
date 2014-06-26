/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone, $ */
(function() {
  "use strict";

  window.Graph = Backbone.Model.extend({

    idAttribute: "_key",

    urlRoot: "/_api/gharial",

    isNew: function() {
      return !this.get("_id");
    },

    parse: function(raw) {
      return raw.graph || raw;
    },

    addEdgeDefinition: function(edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: "POST",
          url: this.urlRoot + "/" + this.get("_key") + "/edge",
          data: JSON.stringify(edgeDefinition)
        }
      );
    },

    deleteEdgeDefinition: function(edgeCollection) {
      $.ajax(
        {
          async: false,
          type: "DELETE",
          url: this.urlRoot + "/" + this.get("_key") + "/edge/" + edgeCollection
        }
      );
    },

    modifyEdgeDefinition: function(edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: "PUT",
          url: this.urlRoot + "/" + this.get("_key") + "/edge/" + edgeDefinition.collection,
          data: JSON.stringify(edgeDefinition)
        }
      );
    },


    defaults: {
      name: "",
      edgeDefinitions: [],
      orphanCollections: []
    }
  });
}());
