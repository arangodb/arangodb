/* global Backbone, window, arangoHelper, frontendConfig */

(function () {
  'use strict';

  window.CurrentDatabase = Backbone.Model.extend({
    url: arangoHelper.databaseUrl('/_api/database/current', frontendConfig.db),

    parse: function (data) {
      return data.result;
    }
  });
}());
