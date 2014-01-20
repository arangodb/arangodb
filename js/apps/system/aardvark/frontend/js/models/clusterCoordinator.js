/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCoordinator = Backbone.Model.extend({
    defaults: {
      "name": "",
      "url": ""
    },

    idAttribute: "name",

    url: function() {
      return "/_admin/aardvark/cluster/Coordinators";
    }
    
  });
}());
