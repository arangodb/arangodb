/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global window, Backbone, $,_, window, frontendConfig */

window.ArangoUsers = Backbone.Collection.extend({
  model: window.Users,

  activeUser: null,
  activeUserSettings: {
    "query" : {},
    "shell" : {},
    "testing": true
  },

  sortOptions: {
    desc: false
  },

  url: frontendConfig.basePath + "/_api/user",

  //comparator : function(obj) {
  //  return obj.get("user").toLowerCase();
  //},

  comparator: function(item, item2) {
    var a = item.get('user').toLowerCase();
    var b = item2.get('user').toLowerCase();
    if (this.sortOptions.desc === true) {
      return a < b ? 1 : a > b ? -1 : 0;
    }
    return a > b ? 1 : a < b ? -1 : 0;
  },

  login: function (username, password, callback) {
    var self = this;

    $.ajax("login", {
      method: "POST",
      data: JSON.stringify({
        username: username,
        password: password
      }),
      dataType: "json"
    }).success(
      function (data) {
        self.activeUser = data.user;
        callback(false, self.activeUser);
      }
    ).error(
      function () {
        self.activeUser = null;
        callback(true, null);
      }
    );
  },

  setSortingDesc: function(yesno) {
    this.sortOptions.desc = yesno;
  },

  logout: function () {
    $.ajax("logout", {method:"POST"});
    this.activeUser = null;
    this.reset();
    window.App.navigate("");
    window.location.reload();
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function (callback) {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: frontendConfig.basePath + "/_api/user/" + encodeURIComponent(self.activeUser),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.activeUserSettings = data.extra;
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },

  saveUserSettings: function (callback) {
    var self = this;
    $.ajax({
      cache: false,
      type: "PUT",
      url: frontendConfig.basePath + "/_api/user/" + encodeURIComponent(self.activeUser),
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },

  parse: function(response)  {
    var result = [];
    _.each(response.result, function(object) {
      result.push(object);
    });
    return result;
  },

  whoAmI: function(callback) {
    if (this.activeUser) {
      callback(false, this.activeUser);
      return;
    }
    $.ajax("whoAmI?_=" + Date.now())
    .success(
      function(data) {
        callback(false, data.user);
      }
    ).error(
      function(data) {
        callback(true, null);
      }
    );
  }


});
