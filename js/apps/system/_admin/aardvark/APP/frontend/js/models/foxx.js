/*jshint browser: true */
/*global Backbone, $, arango */
(function () {
  "use strict";

  var sendRequest = function (foxx, callback, type, part, body) {
    var req = {
      contentType: "application/json",
      processData: false,
      success: function (data) {
        if (typeof callback === "function") {
          callback(null, data);
        }
      },
      error: function (err) {
        if (typeof callback === "function") {
          callback(err);
        }
      }
    };
    req.type = type;
    if (part) {
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
    idAttribute: "mount",

    defaults: {
      "author": "Unknown Author",
      "name": "",
      "version": "Unknown Version",
      "description": "No description",
      "license": "Unknown License",
      "contributors": [],
      "scripts": {},
      "config": {},
      "deps": {},
      "git": "",
      "system": false,
      "development": false
    },

    isNew: function () {
      return false;
    },

    encodedMount: function () {
      return encodeURIComponent(this.get("mount"));
    },

    destroy: function (callback) {
      sendRequest(this, callback, "DELETE");
    },

    isBroken: function () {
      return false;
    },

    needsAttention: function () {
      return this.isBroken() || this.needsConfiguration() || this.hasUnconfiguredDependencies();
    },

    needsConfiguration: function () {
      return _.any(this.get('config'), function (cfg) {
        return cfg.current === undefined;
      });
    },

    hasUnconfiguredDependencies: function () {
      return _.any(this.get('deps'), function (dep) {
        return dep.current === undefined;
      });
    },

    getConfiguration: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("config", data);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "GET", "config");
    },

    setConfiguration: function (data, callback) {
      sendRequest(this, callback, "PATCH", "config", data);
    },

    getDependencies: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("deps", data);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "GET", "deps");
    },

    setDependencies: function (data, callback) {
      sendRequest(this, callback, "PATCH", "deps", data);
    },

    toggleDevelopment: function (activate, callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("development", activate);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "PATCH", "devel", activate);
    },

    runScript: function (name, callback) {
      sendRequest(this, callback, "POST", "scripts/" + name);
    },

    runTests: function (options, callback) {
      sendRequest(this, function (err, data) {
        if (typeof callback === "function") {
          callback(err ? err.responseJSON : err, data);
        }
      }.bind(this), "POST", "tests", options);
    },

    isSystem: function () {
      return this.get("system");
    },

    isDevelopment: function () {
      return this.get("development");
    },

    download: function () {
      window.open(
        "/_db/" + arango.getDatabaseName() + "/_admin/aardvark/foxxes/download/zip?mount=" + this.encodedMount()
      );
    }
  });
}());
