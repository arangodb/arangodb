/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone, $, window, _*/

window.ArangoDatabase = Backbone.Collection.extend({
  model: window.Database,

  url: function() {
    return '/_api/database/';
  },

  parse: function(response) {
    return _.map(response.result, function(v) {
      return {name:v};
    });
  },

  initialize: function() {
    this.fetch();
  },

  getDatabases: function() {
    this.fetch();
    return this.models;
  },

  getCurrentDatabase: function() {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/database/current",
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
