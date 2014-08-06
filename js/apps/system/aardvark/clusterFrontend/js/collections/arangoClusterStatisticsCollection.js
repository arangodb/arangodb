/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window */
window.ClusterStatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,

  url: "/_admin/statistics",

  updateUrl: function() {
    this.url = window.App.getNewRoute("statistics");
  },

  initialize: function() {
    window.App.registerForUpdate(this);
  },

  // The callback has to be invokeable for each result individually
  fetch: function(callback, errCB) {
    this.forEach(function (m) {
      m.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          errCB(m);
        }
      }).done(function() {
        callback(m);
      });
    });
  }
});
