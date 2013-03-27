window.FoxxActiveView = Backbone.View.extend({
  tagName: 'li',
  className: "span3",
  template: new EJS({url: '/_admin/html/js/templates/foxxActiveView.ejs'}),
  
  events: {
    'click .icon-edit': 'editFoxx'
  },
  
  initialize: function(){
    _.bindAll(this, 'render');
  },
  
  editFoxx: function() {
    alert("Functionality will be added soon: You will be able to configure your foxx here.")
  },
    
  render: function(){
    $(this.el).html(this.template.render(this.model));
    return $(this.el);
  }
});
