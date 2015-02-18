/*jshint browser: true */
/*global Backbone, $ */
(function() {
  "use strict";

  var sendRequest = function (foxx, callback, type, part, body) {
    var req = {
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(data);
      },
      error: function(err) {
        callback(err);
      }
    };
    req.type = type;
    if (part && part !== "") {
      req.url = "/_admin/aardvark/foxxes/" + part + "?mount=" + foxx.encodedMount();
    } else {
      req.url = "/_admin/aardvark/foxxes?mount=" + foxx.encodedMount();
    }
    if (body !== undefined) {
      req.data = JSON.stringify(body);
    }
    $.ajax(req);
  };

  window.Foxx = Backbone.Model.extend({
    idAttribute: 'mount',

    defaults: {
      "author": "Unknown Author",
      "name": "",
      "version": "Unknown Version",
      "description": "No description",
      "license": "Unknown License",
      "contributors": [],
      "git": "",
      "system": false,
      "development": false
    },

    isNew: function() {
      return false;
    },

    encodedMount: function() {
      return encodeURIComponent(this.get("mount"));
    },

    destroy: function(callback) {
      sendRequest(this, callback, "DELETE");
    },

    getConfiguration: function(callback) {
      sendRequest(this, callback, "GET", "config");
    },

    setConfiguration: function(data, callback) {
      sendRequest(this, callback, "PATCH", "config", data);
    },

    toggleDevelopment: function(activate, callback) {
      $.ajax({
        type: "PATCH",
        url: "/_admin/aardvark/foxxes/devel?mount=" + this.encodedMount(),
        data: JSON.stringify(activate),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          this.set("development", activate);
          callback(data);
        }.bind(this),
        error: function(err) {
          callback(err);
        }
      });
    },

    setup: function(callback) {
      sendRequest(this, callback, "PATCH", "setup");
    },

    teardown: function(callback) {
      sendRequest(this, callback, "PATCH", "teardown");
    },

    isSystem: function() {
      return this.get("system");
    },

    isDevelopment: function() {
      return this.get("development");
    }

  });
}());
