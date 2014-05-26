/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = Backbone.Collection.extend({
    model: window.ClusterCoordinator,
    
    url: "/_admin/aardvark/cluster/Coordinators",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Coordinators");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
      this._retryCount = 0;
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning": 
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default: 
          return "danger";
      }
    },
  
    getStatuses: function(cb, nextStep) {
      if (this._retryCount > 10) {
        console.log("Cluster unreachable");
      }
      var self = this;
      this.updateUrl();
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          self._retryCount++;
          console.log(self._retryCount);
          self.forEach(function(m) {
            cb(self.statusClass("critical"), m.get("address"));
          });
          window.App.clusterPlan.rotateCoordinator();
          self.getStatuses(cb, nextStep);
        }
      }).done(function() {
        console.log(self._retryCount);
        self._retryCount = 0;
        self.forEach(function(m) {
          cb(self.statusClass(m.get("status")), m.get("address"));
        });
        nextStep();
      });
    },

    byAddress: function (res, callback) {
      var self = this;
      this.updateUrl();
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          console.log("incord2");
        }
      }).done(function() {
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].coords = res[addr].coords || [];
          res[addr].coords.push(m);
        });
        callback(res);
      });
    },

    getList: function() {
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      return this.map(function(m) {
        return m.forList();
      });
    },

    getOverview: function() {
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
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


