/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $, arangoHelper */
(function () {
  "use strict";

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,

    sortOptions: {
      desc: false
    },

    url: "/_api/gharial",

    dropAndDeleteGraph: function(name, callback) {
      $.ajax({
        type: "DELETE",
        url: "/_api/gharial/" + encodeURIComponent(name) + "?dropCollections=true",
        contentType: "application/json",
        processData: true,
        async: false,
        success: function() {
          arangoHelper.arangoNotification("Graph deleted.");
          callback(true);
          return true;
        },
        error: function(data) {
          try {
            var errorMsg = JSON.parse(data.responseText).errorMessage;
            arangoHelper.arangoError("Graph", errorMsg);
          }
          catch (e) {
            arangoHelper.arangoError("Graph", "Could not delete Graph.");
          }
          callback(false);
          return false;
        }
      });
    },

    comparator: function(item, item2) {
      var a = item.get('_key') || "";
      var b = item2.get('_key') || "";
      a = a.toLowerCase();
      b = b.toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function(val) {
      this.sortOptions.desc = val;
    },

    parse: function(res) {
      if (!res.error) {
        return res.graphs;
      }
    }
  });
}());
