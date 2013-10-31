/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone, $, window */

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

  dropDatabase: function() {

  },

  createDatabse: function() {

  }
});
