/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterShard = Backbone.Model.extend({
    defaults: {
      "id": "",
      "status": "ok"
    },

    idAttribute: "id",

    forList: function() {
      return {
        id: this.get("id"),
        status: this.get("status")
      };
    },

    url: function() {
      return "/_admin/aardvark/cluster/Shards";
    }
    
  });
}());

