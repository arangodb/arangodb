var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',
  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionsView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);

    this.collection.each(function (arango_collection) {
      $('.thumbnails', this.el).append(new window.CollectionListItemView({model: arango_collection}).render().el);
    }, this);

    return this;
  },
  events: {
    "click .icon-info-sign" : "details"
  },
  details: function() {
    console.log(this);
  }

});
