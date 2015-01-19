/*global window, Backbone, _, console */
(function() {

  "use strict";

  window.ClusterServers = window.AutomaticRetryCollection.extend({

    model: window.ClusterServer,

    url: "/_admin/aardvark/cluster/DBServers",

    updateUrl: function() {
      this.url = window.App.getNewRoute("DBServers");
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

    getStatuses: function(cb) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this,
        completed = function() {
          self.successFullTry();
          self._retryCount = 0;
          self.forEach(function(m) {
            cb(self.statusClass(m.get("status")), m.get("address"));
          });
        };
      // This is the first function called in
      // Each update loop
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(completed);
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
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
          res[addr].dbs = res[addr].dbs || [];
          res[addr].dbs.push(m);
        });
        callback(res);
      });
    },

    getList: function(callback) {
      throw "Do not use";
      /*
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        var res = [];
        _.each(self.where({role: "primary"}), function(m) {
          var e = {};
          e.primary = m.forList();
          if (m.get("secondary")) {
            e.secondary = self.get(m.get("secondary")).forList();
          }
          res.push(e);
        });
        callback(res);
      });
      */
    },

    getOverview: function() {
      throw "Do not use DbServer.getOverview";
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
      self = this,
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      _.each(this.where({role: "primary"}), function(m) {
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
            var bkp = self.get(m.get("secondary"));
            if (!bkp || bkp.get("status") === "critical") {
              updateStatus("critical");
            } else {
              if (bkp.get("status") === "ok") {
                res.having++;
                updateStatus("warning");
              }
            }
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

