/*global Backbone, window, arangoHelper */

(function() {
  "use strict";

  window.CurrentDatabase = Backbone.Model.extend({
    url: arangoHelper.databaseUrl("/_api/database/current"),

    parse: function(data) {
      return data.result;
    }
  });
}());
