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
