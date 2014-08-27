/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global require, exports, Backbone, window */
window.StatisticsDescriptionCollection = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "/_admin/statistics-description",
  parse: function(response) {
    return response;
  }
});
