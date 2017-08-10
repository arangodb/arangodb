/* jshint browser: true */
/* eslint-env browser */
/* global Backbone, window */
(function () {
  'use strict';

  window.FoxxRepoModel = Backbone.Model.extend({
    idAttribute: 'name',

    defaults: {
      'name': '',
      'description': 'No description',
      'author': 'Unknown Author',
      'latestVersion': 'Unknown Version',
      'legacy': false,
      'category': 'Category',
      'thumbnailUrl': 'https://www.arangodb.com/wp-content/uploads/2013/04/ArangoDB-Foxx-logo-bg.png'
    },

    fetchThumbnail: function (cb) {
      if (cb) {
        cb();
      }
    }

  });
}());
