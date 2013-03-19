window.CollectionListItemView = Backbone.View.extend({

  tagName: "li",
  className: "span3",
  template: new EJS({url: '/_admin/html/js/templates/collectionsItemView.ejs'}),

  initialize: function () {
    //this.model.bind("change", this.render, this);
    //this.model.bind("destroy", this.close, this);
  },
  events: {
  },
  render: function () {
    $(this.el).html(this.template.render(this.model));
    return this;
  }

});
