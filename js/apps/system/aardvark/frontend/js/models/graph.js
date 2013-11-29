/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.Graph = Backbone.Model.extend({

    idAttribute: "_key",

    url: function() {
      if (this.isNew()) {
        return "/_api/graph/";
      } else {
        return "/_api/graph/" + this.get("_key");
      }
    },

    defaults: {
      "_key": "",
      "_id": "",
      "_rev": "",
      "vertices": "",
      "edges": ""
    }
  });
}());
