/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, arangoHelper, $, frontendConfig */
(function () {
  'use strict';
  window.QueryManagementActive = Backbone.Collection.extend({
    model: window.queryManagementModel,

    url: function () {
      var url = frontendConfig.basePath + '/_api/query/current';

      if (window.frontendConfig.db !== '_system') {
        url = arangoHelper.databaseUrl('/_api/query/current');
      }

      return url;
    },

    killRunningQuery: function (id, callback) {
      var url = frontendConfig.basePath + '/_api/query/' + encodeURIComponent(id);

      if (window.frontendConfig.db !== '_system') {
        url = arangoHelper.databaseUrl('/_api/query/' + encodeURIComponent(id));
      }

      $.ajax({
        url: url,
        type: 'DELETE',
        success: function (result) {
          callback();
        }
      });
    }

  });
}());
