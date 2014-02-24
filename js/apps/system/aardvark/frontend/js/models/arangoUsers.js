/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, $, window */

window.Users = Backbone.Model.extend({
  defaults: {
    user: "",
    active: false,
    extra: {}
  },

  idAttribute : "user",

  parse : function (d) {
    this.isNotNew = true;
    return d;
  },

  isNew: function () {
    return !this.isNotNew;
  },

  url: function () {
    if (this.get("user") !== "") {
      return "/_api/user/" + this.get("user");
    }
    return "/_api/user";
  },

  initialize: function () {
  },

  isAuthorized: function () {
    //return Boolean(this.get("sessionId");
    return true;
  },

  isNotAuthorized: function () {
    return false;
  },

  checkPassword: function(passwd) {
    var self = this,
      result = false;

    $.ajax({
      cache: false,
      type: "POST",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data.result;
      },
      error: function(data) {
      }
    });
    return result;
  },

  setPassword: function(passwd) {
    var self = this,
      result = false;

    $.ajax({
      cache: false,
      type: "PATCH",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
      },
      error: function(data) {
      }
    });
  }

});
