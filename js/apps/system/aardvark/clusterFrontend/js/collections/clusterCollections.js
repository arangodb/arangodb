/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.ClusterCollections = window.AutomaticRetryCollection.extend({
    model: window.ClusterCollection,
    
    updateUrl: function() {
      this.url = window.App.getNewRoute(this.dbname + "/Collections");
    },

    url: function() {
      return "/_admin/aardvark/cluster/"
        + this.dbname + "/"
        + "Collections";
    },

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
      window.App.registerForUpdate(this);
    },

    getList: function(db, callback) {
      if (db === undefined) {
        return;
      }
      this.dbname = db;
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, db, callback))
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



