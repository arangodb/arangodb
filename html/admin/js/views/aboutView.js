var aboutView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },

  template: new EJS({url: 'js/templates/aboutView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $.gritter.removeAll();
    return this;
  }

});
