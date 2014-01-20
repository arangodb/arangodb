/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCoordinators = Backbone.Collection.extend({
    model: window.ClusterCoordinator,
    
    url: "/_admin/aardvark/cluster/Coordinators",

    getList: function() {
     return [
        {
          name: "Charly",
          url: "tcp://192.168.0.1:1337",
          status: "ok"
        },
        {
          name: "Carlos",
          url: "tcp://192.168.0.2:1337",
          status: "critical"
        },
        {
          name: "Chantalle",
          url: "tcp://192.168.0.5:1337",
          status: "ok"
        }
      ];
    },

    getOverview: function() {
      // Fake data
      return {
        plan: 3,
        having: 2,
        status: "critical"
      };
    }

  });
}());


