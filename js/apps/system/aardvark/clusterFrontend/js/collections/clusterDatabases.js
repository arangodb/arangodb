/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {

  "use strict";

  window.ClusterDatabases = window.AutomaticRetryCollection.extend({

    model: window.ClusterDatabase,
    
    url: "/_admin/aardvark/cluster/Databases",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Databases");
    },

    initialize: function() {
      window.App.registerForUpdate(this);
    },

    getList: function(callback) {
      this.checkRetries();
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback(self.map(function(m) {
          return m.forList();
        }));
      });
    }

  });
}());
