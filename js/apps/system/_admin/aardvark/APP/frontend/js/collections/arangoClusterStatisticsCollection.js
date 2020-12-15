/* global Backbone, window */
/* jshint strict: false */

window.ClusterStatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,

  url: '/_admin/statistics',

  updateUrl: function () {
    this.url = window.App.getNewRoute(this.host) + this.url;
  },

  initialize: function (models, options) {
    this.host = options.host;
    window.App.registerForUpdate(this);
  }

  // The callback has to be invokeable for each result individually
  // TODO RE-ADD Auth
/* fetch: function(callback, errCB) {
  this.forEach(function (m) {
    m.fetch({
      beforeSend: window.App.addAuth.bind(window.App),
      error: function() {
        errCB(m)
      }
    }).done(function() {
      callback(m)
    })
  })
} */
});
