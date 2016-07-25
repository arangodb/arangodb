/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone */
(function () {
  'use strict';
  window.WorkMonitorCollection = Backbone.Collection.extend({
    model: window.workMonitorModel,

    url: '/_admin/work-monitor',

    parse: function (response) {
      return response.work;
    }

  });
}());
