/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.arangoCollection = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },

  urlRoot: "/_api/collection",
  defaults: {
    id: "",
    name: "",
    status: "",
    type: "",
    isSystem: false,
    picture: ""
  }

});
