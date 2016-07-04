/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, $ */
(function () {
  'use strict';
  window.QueryManagementSlow = Backbone.Collection.extend({
    model: window.queryManagementModel,
    url: '/_api/query/slow',

    deleteSlowQueryHistory: function (callback) {
      var self = this;
      $.ajax({
        url: self.url,
        type: 'DELETE',
        success: function (result) {
          callback();
        }
      });
    }
  });
}());
