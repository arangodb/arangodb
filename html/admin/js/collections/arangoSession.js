/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone */

window.ArangoSession = Backbone.Collection.extend({
  model: window.Session,

  activeUser: "",
  activeUserSettings: {
    "query" : {},
    "shell" : {},
    "testing": true
  },
  
  url: "../api/user",

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
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function () {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/user/" + self.activeUser,
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
    console.log(self.activeUserSettings);
    $.ajax({
      cache: false,
      type: "PUT",
      async: false, // sequential calls!
      url: "/_api/user/" + self.activeUser,
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        console.log(data);
      },
      error: function(data) {
        console.log(data);
      }
    });
  }



});
