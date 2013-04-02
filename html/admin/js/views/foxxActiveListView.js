var FoxxActiveListView = Backbone.View.extend({
  el: '#content',
  template: new EJS({url: 'js/templates/foxxListView.ejs'}),
  
  events: {
    // 'click button#add': 'callback'
  },
  
  initialize: function() {    
    this._subViews = {};
    var self = this;
    this.collection.fetch({
      success: function() {
        _.each(self.collection.where({type: "mount"}), function (foxx) {
          var subView = new window.FoxxActiveView({model: foxx});
          self._subViews[foxx.get('_id')] = subView;
        });
        self.render();
      },
      error: function() {
        console.log("Erroreoror!!");
      }
    });
    this.render();
  },
  
  reload: function() {
    var self = this;
    this.collection.fetch({
      success: function() {
        self._subViews = {};
        _.each(self.collection.where({type: "mount"}), function (foxx) {
          var subView = new window.FoxxActiveView({model: foxx});
          self._subViews[foxx.get('_id')] = subView;
        });
        self.render();
      }
    });
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
