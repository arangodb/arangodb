/*jshint browser: true */
/*global Backbone, $ */
(function() {
  "use strict";

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
      $.ajax({
        type: "DELETE",
        url: "/_admin/aardvark/foxxes?mount=" + this.encodedMount(),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    getConfiguration: function(callback) {
      $.ajax({
        type: "GET",
        url: "/_admin/aardvark/foxxes/config?mount=" + this.encodedMount(),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    setConfiguration: function(data, callback) {
      $.ajax({
        type: "PATCH",
        url: "/_admin/aardvark/foxxes/config?mount=" + this.encodedMount(),
        data: JSON.stringify(data),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
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

    isSystem: function() {
      return this.get("system");
    },

    isDevelopment: function() {
      return this.get("development");
    }

  });
}());
