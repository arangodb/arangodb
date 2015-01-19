/*global Backbone, window */

(function() {
  "use strict";

  window.CurrentDatabase = Backbone.Model.extend({
    url: "/_api/database/current",

    parse: function(data) {
      return data.result;
    }
  });
}());
