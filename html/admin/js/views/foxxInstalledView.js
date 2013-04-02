window.FoxxInstalledView = Backbone.View.extend({
  tagName: 'li',
  className: "span3",
  template: new EJS({url: 'js/templates/foxxInstalledView.ejs'}),
  
  events: {
    // 'click button#add': 'callback'
  },
  
  initialize: function(){
    _.bindAll(this, 'render');
  },
  
  render: function(){
    $(this.el).html(this.template.render(this.model));
    return $(this.el);
  }
});
