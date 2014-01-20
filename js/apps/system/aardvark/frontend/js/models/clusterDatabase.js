/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterDatabase = Backbone.Model.extend({
    defaults: {
      "name": ""
    },

    idAttribute: "name",

    url: function() {
      return "/_admin/aardvark/cluster/Databases";
    }
    
  });
}());
