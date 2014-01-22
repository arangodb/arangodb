/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = Backbone.Collection.extend({
    model: window.ClusterCoordinator,
    
    url: "/_admin/aardvark/cluster/Coordinators",

    getList: function() {
      this.fetch({
        async: false
      });
      return this.map(function(m) {
        return m.forList();
      });
    },

    getOverview: function() {
      this.fetch({
        async: false
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      this.each(function(m) {
        res.plan++; 
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            updateStatus("critical");
            break;
          default:
            console.debug("Undefined server state occured. This is still in development");
        }
      });
      return res;
    }

  });
}());


