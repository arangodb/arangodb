/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function () {
  "use strict";

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,
    
    url: "/_api/graph",

    parse: function(res) {
      if (!res.error) {
        return res.graphs;
      }
    }
  });
}());
