/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterDatabases = Backbone.Collection.extend({

    model: window.ClusterDatabase,
    
    url: "/_admin/aardvark/cluster/Databases",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Databases");
    },

    initialize: function(options) {
      this.isUpdating = false;
      this.timer = null;
      this.interval = options.interval;
      window.App.registerForUpdate(this);
    },

    getList: function() {
      this.fetch({
        async: false
      });
      return this.map(function(m) {
        return m.forList();
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
        self.fetch();
      }, this.interval);
    }

  });
}());


