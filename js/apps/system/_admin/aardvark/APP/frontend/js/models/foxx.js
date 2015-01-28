/*global window, Backbone */
(function() {
  "use strict";

  window.Foxx = Backbone.Model.extend({
    idAttribute: 'mount',

    defaults: {
      "author": "Unknown Author",
      "title": "",
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
    }

  });
}());
