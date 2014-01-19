/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterServer = Backbone.Model.extend({
    defaults: {
      name: "",
      address: "",
      role: "",
      status: "ok"
    },

    idAttribute: "name",

    url: function() {
      return "/_admin/aardvark/cluster/DBServers";
    },

    forList: function() {
      return {
        name: this.get("name"),
        address: this.get("address"),
        status: this.get("status")
      };
    }
    
  });
}());

