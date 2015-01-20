/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $, window, _*/

(function() {
  'use strict';

  window.ArangoDatabase = Backbone.Collection.extend({

    model: window.DatabaseModel,

    sortOptions: {
      desc: false
    },

    shouldFetchUser: false,

    comparator: function(item, item2) {
      var a = item.get('name').toLowerCase();
      var b = item2.get('name').toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
      return a > b ? 1 : a < b ? -1 : 0;
    },

    sync: function(method, model, options) {
      if (method === "read") {
        if (this.shouldFetchUser) {
          options.url = model.url() + "user";
        }
      }
      return Backbone.sync(method, model, options);
    },

    url: function() {
      return '/_api/database/';
    },

    parse: function(response) {
      if (!response) {
        return;
      }
      return _.map(response.result, function(v) {
        return {name:v};
      });
    },

    initialize: function(values, options) {
      this.shouldFetchUser = options.shouldFetchUser;
      var self = this;
      this.fetch().done(function() {
        self.sort();
      });
    },

    setSortingDesc: function(yesno) {
      this.sortOptions.desc = yesno;
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
        + "/_admin/aardvark/standalone.html";
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
