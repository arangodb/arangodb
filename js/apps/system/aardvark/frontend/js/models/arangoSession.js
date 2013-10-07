/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, $, window */

window.Session = Backbone.Model.extend({
  defaults: {
    sessionId: "",
    userName: "",
    password: "",
    userId: "",
    data: {}
  },

  initialize: function () {
  },

  isAuthorized: function () {
    //return Boolean(this.get("sessionId");
    return true;
  },

  isNotAuthorized: function () {
    return false;
  }

});
