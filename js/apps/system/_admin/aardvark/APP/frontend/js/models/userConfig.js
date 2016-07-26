/* jshint strict: false */
/* global Backbone, window, arangoHelper, $ */
window.UserConfig = Backbone.Model.extend({
  defaults: {
    graphs: '',
    queries: []
  },

  model: window.UserConfigModel,

  parse: function (response) {
    return response.result;
  },

  url: function () {
    if (window.App.currentUser) {
      this.username = window.App.currentUser;
    } else {
      this.username = 'root';
    }

    return arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.username) + '/config');
  },

  setItem: function (keyName, keyValue, callback) {
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
  },

  getItem: function (keyName, callback) {
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

});
