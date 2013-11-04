window.DBSelectionView = Backbone.View.extend({
  el: '#selectDB',

  template: templateEngine.createTemplate("dbSelectionView.ejs"),

  initialize: function() {
    this.collection.fetch();
    this.current = this.collection.getCurrentDatabase();
  },

  render: function() {
    $(this.el).html(this.template.render({
      list: this.collection,
      current: this.current
    }));
    console.log($(this.el).html());
    return $(this.el);
  }
});
