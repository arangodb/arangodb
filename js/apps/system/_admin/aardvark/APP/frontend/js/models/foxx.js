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
    }

  });
}());
