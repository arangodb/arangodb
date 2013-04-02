var footerView = Backbone.View.extend({
  el: '.footer',
  init: function () {
  },

  template: new EJS({url: 'js/templates/footerView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
  }

});
