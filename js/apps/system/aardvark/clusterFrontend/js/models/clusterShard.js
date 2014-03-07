/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterShard = Backbone.Model.extend({
    defaults: {
    },

    idAttribute: "name",

    forList: function() {
      return {
        server: this.get("name"),
        shards: this.get("shards")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Shards";

    updateUrl: function() {
      this.url = window.getNewRoute("Shards");
    }
    */
  });
}());

