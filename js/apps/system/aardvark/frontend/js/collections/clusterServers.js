/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterServers = Backbone.Collection.extend({
    model: window.ClusterServer,
    
    url: "/_admin/aardvark/cluster/DBServers",

    getList: function() {
      return [
        {
          primary: {
            name: "Pavel",
            url: "tcp://192.168.0.1:1337",
            status: "ok"
          },
          secondary: {
            name: "Sally",
            url: "tcp://192.168.1.1:1337",
            status: "ok"
          }
        },
        {
          primary: {
            name: "Pancho",
            url: "tcp://192.168.0.2:1337",
            status: "ok"
          }
        },
        {
          primary: {
            name: "Pablo",
            url: "tcp://192.168.0.5:1337",
            status: "critical"
          },
          secondary: {
            name: "Sandy",
            url: "tcp://192.168.1.5:1337",
            status: "critical"
          }
        }
      ];
    },

    getOverview: function() {
      // Fake data
      return {
        plan: 3,
        having: 3,
        status: "warning"
      };
    }

  });
}());

