/* jshint browser: true */
/* jshint strict: false, unused: false */
/* global window, atob, Backbone, $,_, window, frontendConfig, arangoHelper */

window.ArangoUsers = Backbone.Collection.extend({
  model: window.Users,

  activeUser: null,
  activeUserSettings: {
    'query': {},
    'shell': {},
    'testing': true
  },

  sortOptions: {
    desc: false
  },

  fetch: function (options) {
    if (window.App.currentUser && window.App.currentDB.get('name') !== '_system') {
      this.url = frontendConfig.basePath + '/_api/user/' + encodeURIComponent(window.App.currentUser);
    }
    return Backbone.Collection.prototype.fetch.call(this, options);
  },

  url: frontendConfig.basePath + '/_api/user',

  // comparator : function(obj) {
  //  return obj.get("user").toLowerCase()
  // },

  comparator: function (item, item2) {
    var a = item.get('user').toLowerCase();
    var b = item2.get('user').toLowerCase();
    if (this.sortOptions.desc === true) {
      return a < b ? 1 : a > b ? -1 : 0;
    }
    return a > b ? 1 : a < b ? -1 : 0;
  },

  login: function (username, password, callback) {
    var self = this;

    $.ajax({
      url: arangoHelper.databaseUrl('/_open/auth'),
      method: 'POST',
      data: JSON.stringify({
        username: username,
        password: password
      }),
      dataType: 'json'
    }).success(
      function (data) {
        arangoHelper.setCurrentJwt(data.jwt);

        var jwtParts = data.jwt.split('.');
        if (!jwtParts[1]) {
          throw new Error('Invalid JWT');
        }

        if (!window.atob) {
          throw new Error('base64 support missing in browser');
        }
        var payload = JSON.parse(atob(jwtParts[1]));

        self.activeUser = payload.preferred_username;
        callback(false, self.activeUser);
      }
    ).error(
      function () {
        arangoHelper.setCurrentJwt(null);
        self.activeUser = null;
        callback(true, null);
      }
    );
  },

  setSortingDesc: function (yesno) {
    this.sortOptions.desc = yesno;
  },

  logout: function () {
    arangoHelper.setCurrentJwt(null);
    this.activeUser = null;
    this.reset();
    window.App.navigate('');
    window.location.reload();
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function (callback) {
    var self = this;

    $.ajax({
      type: 'GET',
      cache: false,
      // url: frontendConfig.basePath + "/_api/user/" + encodeURIComponent(self.activeUser),
      url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.activeUser)),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        self.activeUserSettings = data.extra;
        callback(false, data);
      },
      error: function (data) {
        callback(true, data);
      }
    });
  },

  saveUserSettings: function (callback) {
    var self = this;
    $.ajax({
      cache: false,
      type: 'PUT',
      url: frontendConfig.basePath + '/_api/user/' + encodeURIComponent(self.activeUser),
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data);
      },
      error: function (data) {
        callback(true, data);
      }
    });
  },

  parse: function (response) {
    var result = [];
    if (response.result) {
      _.each(response.result, function (object) {
        result.push(object);
      });
    } else {
      result.push({
        user: response.user,
        active: response.active,
        extra: response.extra,
        changePassword: response.changePassword
      });
    }
    return result;
  },

  whoAmI: function (callback) {
    if (this.activeUser) {
      callback(false, this.activeUser);
      return;
    }
    $.ajax('whoAmI?_=' + Date.now())
      .success(
        function (data) {
          callback(false, data.user);
        }
    ).error(
      function () {
        callback(true, null);
      }
    );
  }

});
