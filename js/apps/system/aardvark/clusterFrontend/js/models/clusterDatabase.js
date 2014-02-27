/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterDatabase = Backbone.Model.extend({

    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
    /*
    url: "/_admin/aardvark/cluster/Databases";

    updateUrl: function() {
      this.url = window.getNewRoute("Databases");
    }
    */
    
  });
}());
