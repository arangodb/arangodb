/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterShards = window.AutomaticRetryCollection.extend({

    model: window.ClusterShard,
    
    updateUrl: function() {
      this.url = window.App.getNewRoute(
        this.dbname + "/" + this.colname + "/Shards"
      );
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + this.colname + "/"
        + "Shards";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(dbname, colname, callback) {
      if (dbname === undefined || colname === undefined) {
        console.log("Called to early");
        return;
      }
      this.dbname = dbname;
      this.colname = colname;
      this.checkRetries();
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(
          self, dbname, colname, callback)
        )
      }).done(function() {
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
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
        self.updateUrl();
        self.fetch({
          beforeSend: window.App.addAuth.bind(window.App)
        });
      }, this.interval);
    }

  });
}());



