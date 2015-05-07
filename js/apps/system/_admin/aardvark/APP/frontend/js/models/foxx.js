/*jshint browser: true */
/*global Backbone, $, _, arango */
(function () {
  "use strict";

  var sendRequest = function (foxx, callback, method, part, body, args) {
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
    req.type = method;
    args = _.extend({mount: foxx.encodedMount()}, args);
    var qs = _.reduce(args, function (base, value, key) {
      return base + encodeURIComponent(key) + '=' + encodeURIComponent(value) + '&';
    }, '?');
    req.url = "/_admin/aardvark/foxxes" + (part ? '/' + part : '') + qs.slice(0, qs.length - 1);
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

    destroy: function (opts, callback) {
      sendRequest(this, callback, "DELETE", undefined, undefined, opts);
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
