var navigationView = Backbone.View.extend({
  el: '.header',
  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/navigationView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
  },
  selectMenuItem: function (menuItem) {                                                                                                                                                          
    $('.nav li').removeClass('active');
    if (menuItem) {
      $('.' + menuItem).addClass('active');
    }
  }

});
