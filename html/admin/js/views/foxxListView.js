var FoxxListView = Backbone.View.extend({
  el: '#content',
  template: new EJS({url: '/_admin/html/js/templates/foxxListView.ejs'}),
  
  events: {
    // 'click button#add': 'callback'
  },
  
  initialize: function() {    
    this._subViews = {};
    var self = this;
    this.collection.fetch({
      success: function() {
        self.collection.each(function (foxx) {
          var subView = new window.FoxxView({model: foxx});
          self._subViews[foxx.get('_id')] = subView;
        });
        self.render();
      }
    });
    this.render();
  },
  
  
  render: function() {
    $(this.el).html(this.template.text);
    var self = this;
    _.each(this._subViews, function (v) {
      $("#foxxList").append(v.render());
    });
    this.delegateEvents();
    return this;
  }
});
