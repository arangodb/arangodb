/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone, $, window, _*/

(function() {
  window.ArangoDatabase = Backbone.Collection.extend({
    model: window.Database,

    comparator: "name",

    sync: function(method, model, options) {
      'use strict';
      if (method === "read") {
        options.url = model.url() + "user";
      }
      return Backbone.sync(method, model, options);
    },

    url: function() {
      return '/_api/database/';
    },

    parse: function(response) {
      return _.map(response.result, function(v) {
        return {name:v};
      });
    },

    initialize: function() {
      var self = this; 
      this.fetch().done(function() {
        self.sort();  
      });
    },

    getDatabases: function() {
      var self = this; 
      this.fetch().done(function() {
        self.sort();  
      });
      return this.models;
    },

    createDatabaseURL: function(name, protocol, port) {
      var loc = window.location;
      var hash = window.location.hash;
      if (protocol) {
        if (protocol === "SSL" || protocol === "https:") {
          protocol = "https:";
        } else {
          protocol = "http:"; 
        }
      } else {
        protocol = loc.protocol;
      }
      port = port || loc.port;

      var url = protocol
        + "//"
        + window.location.hostname
        + ":"
        + port
        + "/_db/"
        + encodeURIComponent(name)
        + "/_admin/aardvark/index.html";
      if (hash) {
        var base = hash.split("/")[0];
        if (base.indexOf("#collection") === 0) {
          base = "#collections";
        }
        if (base.indexOf("#application") === 0) {
          base = "#applications";
        }
        url += base;
      }
      return url;
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
          if (data.code === 200) {
            returnVal = data.result.name;
            return;
          }
          returnVal = data;
        },
        error: function(data) {
          returnVal = data;
        }
      });
      return returnVal;
    }
  });
}());
