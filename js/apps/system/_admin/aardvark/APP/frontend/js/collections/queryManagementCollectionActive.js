/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $, frontendConfig */
(function() {
  "use strict";
  window.QueryManagementActive = Backbone.Collection.extend({

    model: window.queryManagementModel,

    url: function() {
      return frontendConfig.basePath + '/_api/query/current';
    },

    killRunningQuery: function(id, callback) {
      var self = this;
      $.ajax({
        url: frontendConfig.basePath + '/_api/query/'+encodeURIComponent(id),
        type: 'DELETE',
        success: function(result) {
          callback();
        }
      });
    }

  });
}());
