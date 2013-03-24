window.FoxxView = Backbone.View.extend({
  tagName: 'li',
  className: "span3 foxxAttributeItem",
  template: new EJS({url: '/_admin/html/js/templates/foxxView.ejs'}),
  
  events: {
    // 'click button#add': 'callback'
  },
  
  initialize: function(){
    _.bindAll(this, 'render');
  },
  
  render: function(){
    $(this.el).html(this.template.text);
    return $(this.el);
  }
});
