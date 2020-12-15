/* global window, Backbone */

window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    'figures': '',
    'groups': ''
  },
  url: function () {
    'use strict';

    return '/_admin/statistics-description';
  }

});
