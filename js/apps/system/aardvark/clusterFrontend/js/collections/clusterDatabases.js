/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterDatabases = Backbone.Collection.extend({

    model: window.ClusterDatabase,
    
    url: "cluster/Databases",

    initialize: function() {
      this.isUpdating = false;
      this.timer = null;
      this.interval = 1000;
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


