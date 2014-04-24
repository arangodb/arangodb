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
    if (this.isNew()) {
      return "/_api/user";
    }
    if (this.get("user") !== "") {
      return "/_api/user/" + this.get("user");
    }
    return "/_api/user";
  },

  checkPassword: function(passwd) {
    var result = false;

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
      }
    });
    return result;
  },

  setPassword: function(passwd) {
    $.ajax({
      cache: false,
      type: "PATCH",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false
    });
  },

  setExtras: function(name, img) {
    $.ajax({
      cache: false,
      type: "PATCH",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({"extra": {"name":name, "img":img}}),
      contentType: "application/json",
      processData: false
    });
  }

});
