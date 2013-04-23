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
