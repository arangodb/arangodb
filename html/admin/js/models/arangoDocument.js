/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.arangoDocument = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: ""
  }
});
