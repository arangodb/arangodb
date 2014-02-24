/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = Backbone.Collection.extend({
    model: window.ClusterCoordinator,
    
    url: "/_admin/aardvark/cluster/Coordinators",

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
    },

    byAddress: function (res) {
      this.fetch({
        async: false
      });
      res = res || {};
      this.forEach(function(m) {
        var addr = m.get("address");
        addr = addr.split(":")[0];
        res[addr] = res[addr] || {};
        res[addr].coords = res[addr].coords || [];
        res[addr].coords.push(m);
      });
      return res;
    },

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
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }
      this.isUpdating = true;
      var self = this;
      this.timer = window.setInterval(function() {
        self.fetch();
      }, this.interval);
    }

  });
}());


