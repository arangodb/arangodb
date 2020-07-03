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

  authOptions: {
    ro: false
  },

  fetch: function (options) {
    if (options.fetchAllUsers) {
      // flag to load all user profiles, this only works within _system database and needs at least read access
      this.url = arangoHelper.databaseUrl('/_api/user/');
    } else if (frontendConfig.authenticationEnabled && window.App.currentUser) {
      this.url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(window.App.currentUser));
    } else {
      this.url = arangoHelper.databaseUrl('/_api/user/');
    }
    return Backbone.Collection.prototype.fetch.call(this, options);
  },

  url: frontendConfig.basePath + '/_api/user',

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
      dataType: 'json',
      success: function (data) {
        var jwtParts = data.jwt.split('.');

        if (!jwtParts[1]) {
          throw new Error('Invalid JWT');
        }

        if (!window.atob) {
          throw new Error('base64 support missing in browser');
        }

        var payload = JSON.parse(atob(jwtParts[1]));
        self.activeUser = payload.preferred_username;

        if (self.activeUser === undefined) {
          arangoHelper.setCurrentJwt(data.jwt, null);
        } else {
          arangoHelper.setCurrentJwt(data.jwt, self.activeUser);
        }

        callback(false, self.activeUser);
      },
      error: function () {
        arangoHelper.setCurrentJwt(null, null);
        self.activeUser = null;
        callback(true, null);
      }
    });
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

    var url;
    if (!frontendConfig.authenticationEnabled) {
      url = arangoHelper.databaseUrl('/_api/user/root');
    } else {
      url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.activeUser));
    }

    $.ajax({
      type: 'GET',
      cache: false,
      // url: frontendConfig.basePath + "/_api/user/" + encodeURIComponent(self.activeUser),
      url: url,
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
    var self = this;

    if (this.activeUser) {
      callback(false, this.activeUser);
      return;
    }

    var url = 'whoAmI?_=' + Date.now();
    if (frontendConfig.react) {
      url = arangoHelper.databaseUrl('/_admin/aardvark/' + url);
    }

    $.ajax({
      type: 'GET',
      url: url,
      success: function (data) {
        self.activeUser = data.user;
        callback(false, data.user);
      },
      error: function () {
        callback(true, null);
      }
    });
  }

});
