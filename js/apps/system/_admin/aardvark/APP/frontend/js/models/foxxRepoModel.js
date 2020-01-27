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
      'categories': 'No category',
      'defaultThumbnailUrl': 'img/ArangoDB-Foxx-logo-bg.png'
    },

    httpGetAsync: function (theUrl, callback, callbackscnd) {
      var xmlHttp = new XMLHttpRequest();
      xmlHttp.onreadystatechange = function () {
        if (xmlHttp.readyState === 4 && xmlHttp.status === 200) {
          if (callbackscnd) {
            callback(xmlHttp.responseText, callbackscnd);
          } else {
            callback(xmlHttp.responseText);
          }
        } else if (xmlHttp.readyState === 4 && xmlHttp.status === 404) {
          callback();
        }
      };
      xmlHttp.open('GET', theUrl, true); // true for asynchronous
      xmlHttp.send(null);
    },

    manifest: null,

    fetchManifest: function (cb, cbscnd) {
      var self = this;
      var continueFunction = function (manifest, cb) {
        try {
          var maniObject = JSON.parse(manifest);
          if (typeof maniObject === 'object') {
            self.set('manifest', maniObject);
          }
          cb();
          cbscnd();
        } catch (err) {
          // we we're not able to fetch/parse the manifest
          if (cb) {
            cb(true);
          }
          if (cbscnd) {
            cbscnd(true);
          }
        }
      };

      if (!this.manifest) {
        var url = 'https://raw.githubusercontent.com/' + self.get('location') + '/v' + self.get('latestVersion') + '/manifest.json';
        this.httpGetAsync(url, continueFunction, cb);
      } else {
        if (cb) {
          cb();
        }
      }
    },

    fetchReadme: function (cb) {
      this.httpGetAsync(this.getReadmeUrl(), cb);
    },

    getReadmeUrl: function () {
      return 'https://raw.githubusercontent.com/' + this.get('location') + '/v' + this.get('latestVersion') + '/README.md';
    },

    getThumbnailUrl: function () {
      return 'https://raw.githubusercontent.com/' + this.get('location') + '/v' + this.get('latestVersion') + '/' + this.get('manifest').thumbnail;
    },

    fetchThumbnail: function (cb) {
      var self = this;
      var continueFunction = function (cb) {
        if (cb) {
          cb(self.manifest.thumbnail);
        }
      };

      if (!this.manifest) {
        this.fetchManifest(continueFunction, cb);
      } else {
        continueFunction(cb);
      }
    }

  });
}());
