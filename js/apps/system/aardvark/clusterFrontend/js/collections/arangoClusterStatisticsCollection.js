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

  fetch: function() {
     this.forEach(function (m) {
         m.fetch({async: false});
     })
  }
});
