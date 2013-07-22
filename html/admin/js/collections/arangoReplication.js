/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone, $, window */

window.ArangoReplication = Backbone.Collection.extend({
  model: window.Replication,

  url: "../api/user",

  getLogState: function () {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/log-state",
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
      url: "/_api/replication/apply-state",
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
