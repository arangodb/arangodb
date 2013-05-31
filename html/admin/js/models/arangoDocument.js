/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone*/

window.arangoDocument = Backbone.Model.extend({
  initialize: function () {
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: ""
  }
});
