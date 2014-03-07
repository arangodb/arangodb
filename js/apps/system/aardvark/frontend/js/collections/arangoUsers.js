/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone, $,_, window */

window.ArangoUsers = Backbone.Collection.extend({
  model: window.Users,

  activeUser: "",
  activeUserSettings: {
    "query" : {},
    "shell" : {},
    "testing": true
  },

  url: "/_api/user",

  comparator : function(obj) {
    return obj.get("user").toLowerCase();
  },

  initialize: function() {
    //check cookies / local storage
  },

  login: function (username, password) {
    this.activeUser = username;
    return true;
  },

  logout: function () {
    this.activeUser = undefined;
    this.reset();
    $.ajax("unauthorized", {async:false}).error(
      function () {
        window.App.navigate("");
        window.location.reload();
      }
    );
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function () {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        self.activeUserSettings = data.extra;
      },
      error: function(data) {
      }
    });
  },

  saveUserSettings: function () {
    var self = this;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false, // sequential calls!
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
      },
      error: function(data) {
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

  whoAmI: function() {
    if (this.activeUser) {
      return this.activeUser;
    }
    var result;
    $.ajax("whoAmI", {async:false}).done(
      function(data) {
        result = data.name;
      }
    );
    this.activeUser = result;
    return this.activeUser;
  }


});
