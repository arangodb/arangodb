/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone */
(function() {
  "use strict";
  window.QueryManagementActive = Backbone.Collection.extend({

    model: window.queryManagementModel,

    url: function() {
      return '/_api/query/current';
    }

  });
}());
