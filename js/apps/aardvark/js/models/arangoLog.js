/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.arangoLog = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: "/_admin/log",
  defaults: {
    lid: "",
    level: "",
    timestamp: "",
    text: "",
    totalAmount: ""
  }
});
