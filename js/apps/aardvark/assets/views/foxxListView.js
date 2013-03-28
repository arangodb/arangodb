var ItemView = Backbone.View.extend({
  tagName: 'li', 
  
  events: {
    // 'click button#add': 'callback'
  },
  
  initialize: function(){
    _.bindAll(this, 'render');
  },
  
  render: function(){
    $(this.el);
    return this;
  }
});
