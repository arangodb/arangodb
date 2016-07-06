/* jshint browser: true */
/* eslint-env browser */
/* global Backbone, $, _, arangoHelper, window */
(function () {
  'use strict';

  var sendRequest = function (foxx, callback, method, part, body, args) {
    var req = {
      contentType: 'application/json',
      processData: false,
      type: method
    };
    callback = callback || function () {};
    args = _.extend({mount: foxx.encodedMount()}, args);
    var qs = _.reduce(args, function (base, value, key) {
      return base + encodeURIComponent(key) + '=' + encodeURIComponent(value) + '&';
    }, '?');
    req.url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes' + (part ? '/' + part : '') + qs.slice(0, qs.length - 1));
    if (body !== undefined) {
      req.data = JSON.stringify(body);
    }
    $.ajax(req).then(
      function (data) {
        callback(null, data);
      },
      function (xhr) {
        window.xhr = xhr;
        callback(_.extend(
          xhr.status
            ? new Error(xhr.responseJSON ? xhr.responseJSON.errorMessage : xhr.responseText)
            : new Error('Network Error'),
          {statusCode: xhr.status}
        ));
      }
    );
  };

  window.Foxx = Backbone.Model.extend({
    idAttribute: 'mount',

    defaults: {
      'author': 'Unknown Author',
      'name': '',
      'version': 'Unknown Version',
      'description': 'No description',
      'license': 'Unknown License',
      'contributors': [],
      'scripts': {},
      'config': {},
      'deps': {},
      'git': '',
      'system': false,
      'development': false
    },

    isNew: function () {
      return false;
    },

    encodedMount: function () {
      return encodeURIComponent(this.get('mount'));
    },

    destroy: function (options, callback) {
      sendRequest(this, callback, 'DELETE', undefined, undefined, options);
    },

    isBroken: function () {
      return false;
    },

    needsAttention: function () {
      return this.isBroken() || this.needsConfiguration() || this.hasUnconfiguredDependencies();
    },

    needsConfiguration: function () {
      return _.any(this.get('config'), function (cfg) {
        return cfg.current === undefined && cfg.required !== false;
      });
    },

    hasUnconfiguredDependencies: function () {
      return _.any(this.get('deps'), function (dep) {
        return dep.current === undefined && dep.definition.required !== false;
      });
    },

    getConfiguration: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set('config', data);
        }
        if (typeof callback === 'function') {
          callback(err, data);
        }
      }.bind(this), 'GET', 'config');
    },

    setConfiguration: function (data, callback) {
      sendRequest(this, callback, 'PATCH', 'config', data);
    },

    getDependencies: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set('deps', data);
        }
        if (typeof callback === 'function') {
          callback(err, data);
        }
      }.bind(this), 'GET', 'deps');
    },

    setDependencies: function (data, callback) {
      sendRequest(this, callback, 'PATCH', 'deps', data);
    },

    toggleDevelopment: function (activate, callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set('development', activate);
        }
        if (typeof callback === 'function') {
          callback(err, data);
        }
      }.bind(this), 'PATCH', 'devel', activate);
    },

    runScript: function (name, options, callback) {
      sendRequest(this, callback, 'POST', 'scripts/' + name, options);
    },

    runTests: function (options, callback) {
      sendRequest(this, function (err, data) {
        if (typeof callback === 'function') {
          callback(err ? err.responseJSON : err, data);
        }
      }, 'POST', 'tests', options);
    },

    isSystem: function () {
      return this.get('system');
    },

    isDevelopment: function () {
      return this.get('development');
    },

    download: function () {
      sendRequest(this, function (err, data) {
        if (err) {
          console.error(err.responseJSON);
          return;
        }
        window.location.href = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/download/zip?mount=' + this.encodedMount() + '&nonce=' + data.nonce);
      }.bind(this), 'POST', 'download/nonce');
    },

    fetchThumbnail: function (cb) {
      var xhr = new XMLHttpRequest();
      xhr.responseType = 'blob';
      xhr.onload = function () {
        this.thumbnailUrl = URL.createObjectURL(xhr.response);
        cb();
      }.bind(this);
      xhr.onerror = cb;
      xhr.open('GET', 'foxxes/thumbnail?mount=' + this.encodedMount());
      if (window.arangoHelper.getCurrentJwt()) {
        xhr.setRequestHeader('Authorization', 'bearer ' + window.arangoHelper.getCurrentJwt());
      }
      xhr.send();
    }
  });
}());
