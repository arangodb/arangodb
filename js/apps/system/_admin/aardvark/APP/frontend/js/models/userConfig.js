/* jshint strict: false */
/* global _, Backbone, frontendConfig, window, arangoHelper, sessionStorage, $ */
window.UserConfig = Backbone.Model.extend({
  defaults: {
    graphs: '',
    queries: []
  },

  ldapEnabled: false,

  initialize: function (options) {
    this.ldapEnabled = options.ldapEnabled;
    this.getUser();
  },

  model: window.UserConfigModel,

  fetch: function (options) {
    options = _.extend({parse: true}, options);
    var model = this;
    var success = options.success;

    if (this.ldapEnabled) {
      this.getLocalConfig();
      options.success = (function (resp) {
        // if success function available, call it
        if (success) {
          success.call(options.context, model, resp, options);
        }
      })();
    } else {
      // if ldap is not enabled -> call Backbone's fetch method
      return Backbone.Collection.prototype.fetch.call(this, options);
    }
  },

  getConfigPath: function () {
    return frontendConfig.db + '-' + this.username + '-config';
  },

  getLocalConfig: function () {
    var item = sessionStorage.getItem(this.getConfigPath());
    try {
      item = JSON.parse(item);
    } catch (ignore) {
    }

    if (item) {
      this.set(item);
    } else {
      this.set({});
    }
    return this.toJSON();
  },

  saveLocalConfig: function () {
    sessionStorage.setItem(this.getConfigPath(), JSON.stringify(this.toJSON()));
  },

  getLocalItem: function (keyName, callback) {
    if (callback) {
      callback(this.get(keyName));
    }
    return this.get(keyName);
  },

  setLocalItem: function (keyName, keyValue, callback) {
    try {
      this.set(keyName, keyValue);
      this.saveLocalConfig();
      if (callback) {
        callback();
      }
    } catch (ignore) {
    }
  },

  getUser: function () {
    if (window.App.currentUser) {
      this.username = window.App.currentUser;
    } else {
      this.username = 'root';
    }
  },

  parse: function (response) {
    return response.result;
  },

  url: function () {
    return arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config');
  },

  setItem: function (keyName, keyValue, callback) {
    if (this.ldapEnabled) {
      this.setLocalItem(keyName, keyValue, callback);
    } else {
      // url PUT /_api/user/<username>/config/<key>
      var self = this;

      $.ajax({
        type: 'PUT',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config/' + encodeURIComponent(keyName)),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify({value: keyValue}),
        async: true,
        success: function () {
          self.set(keyName, keyValue);

          if (callback) {
            callback();
          }
        },
        error: function () {
          arangoHelper.arangoError('User configuration', 'Could not update user configuration for key: ' + keyName);
        }
      });
    }
  },

  getItem: function (keyName, callback) {
    if (this.ldapEnabled) {
      this.getLocalItem(keyName, callback);
    } else {
      // url GET /_api/user/<username>/config/<key>
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config/' + encodeURIComponent(keyName)),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (keyValue) {
          callback(keyValue);
        },
        error: function () {
          arangoHelper.arangoError('User configuration', 'Could not fetch user configuration for key: ' + keyName);
        }
      });
    }
  }

});
