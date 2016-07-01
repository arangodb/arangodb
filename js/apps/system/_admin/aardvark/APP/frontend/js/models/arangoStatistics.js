/* global window, Backbone */

window.Statistics = Backbone.Model.extend({
  defaults: {
  },

  url: function () {
    'use strict';
    return '/_admin/statistics';
  }
});
