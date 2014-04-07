/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.newArangoLog = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  defaults: {
    lid: "",
    level: "",
    timestamp: "",
    text: "",
    totalAmount: ""
  }
});
