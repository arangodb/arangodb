/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global window, Backbone, $, window */

window.ArangoReplication = Backbone.Collection.extend({
  model: window.Replication,

  url: "../api/user",

  getLogState: function () {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/logger-state",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        returnVal = data;
      },
      error: function(data) {
        returnVal = data;
      }
    });
    return returnVal;
  },
  getApplyState: function () {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/applier-state",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        returnVal = data;
      },
      error: function(data) {
        returnVal = data;
      }
    });
    return returnVal;
  }

});
