/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterDatabase = Backbone.Model.extend({
    defaults: {
      "name": "",
      "id": ""
    },

    idAttribute: "id",

    url: function() {
      return "/_admin/aardvark/cluster/Collections";
    }
    
  });
}());

