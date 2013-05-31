/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone*/

window.arangoCollection = Backbone.Model.extend({
  initialize: function () {
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
