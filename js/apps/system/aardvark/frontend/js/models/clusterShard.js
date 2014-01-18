/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterShard = Backbone.Model.extend({
    defaults: {
      "id": ""
    },

    idAttribute: "id",

    url: function() {
      return "/_admin/aardvark/cluster/Shards";
    }
    
  });
}());

