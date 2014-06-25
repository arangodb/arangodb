/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
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

    defaults: {
      name: "",
      edgeDefinitions: [],
      orphanCollections: []
    }
  });
}());
