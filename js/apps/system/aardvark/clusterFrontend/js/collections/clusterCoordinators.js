/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, console */
(function() {
  "use strict";
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,
    
    url: "/_admin/aardvark/cluster/Coordinators",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Coordinators");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
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
      this.checkRetries();
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb, nextStep))
      }).done(function() {
        self.successFullTry();
        self.forEach(function(m) {
          cb(self.statusClass(m.get("status")), m.get("address"));
        });
        nextStep();
      });
    },

    byAddress: function (res, callback) {
      this.checkRetries();
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
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

    checkConnection: function(callback) {
      var self = this;
      this.checkRetries();
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function() {
        callback();
      });
    },

    getList: function() {
      throw "Do not use coordinator.getList";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      return this.map(function(m) {
        return m.forList();
      });
      */
    },

    getOverview: function() {
      throw "Do not use coordinator.getOverview";
      /*
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
      */
    }
  });
}());


