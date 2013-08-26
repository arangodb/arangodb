/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window */
window.StatisticsDescription = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "../statistics-description",
  parse: function(response) {
    return response;
  }
});
