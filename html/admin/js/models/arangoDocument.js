window.arangoDocument = Backbone.Model.extend({
  initialize: function () {
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: "",
  }
});
