/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, _, console */
(function() {

  "use strict";

  window.ClusterServers = Backbone.Collection.extend({

    model: window.ClusterServer,
    
    url: "/_admin/aardvark/cluster/DBServers",

    getList: function() {
      this.fetch({
        async: false
      });
      var res = [],
          self = this;
      _.each(this.where({role: "primary"}), function(m) {
        var e = {};
        e.primary = m.forList();
        if (m.get("secondary")) {
          e.secondary = self.get(m.get("secondary")).forList();
        }
        res.push(e);
      });
      return res;
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
    }

  });

}());

