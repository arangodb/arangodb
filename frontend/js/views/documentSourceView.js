var documentSourceView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },
  events: {
    "click #tableView"     :   "tableView",
    "click #saveSourceDoc" :   "saveSourceDoc",
  },

  template: new EJS({url: '/_admin/html/js/templates/documentSourceView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
  },
  saveSourceDoc: function() {
    window.arangoDocumentStore.saveDocument("source");
  },
  tableView: function () {
    var hash = window.location.hash.split("/");
    window.location.hash = hash[0]+"/"+hash[1]+"/"+hash[2];
  },
  fillSourceBox: function () {
    var data = arangoDocumentStore.models[0].attributes;
    $('#documentSourceBox').val(JSON.stringify(data));
  }

});
