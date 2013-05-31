/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone*/

window.arangoLog = Backbone.Model.extend({
  initialize: function () {
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
