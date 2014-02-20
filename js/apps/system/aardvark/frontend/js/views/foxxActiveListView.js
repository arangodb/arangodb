/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, _, templateEngine*/
(function () {
  "use strict";
  window.FoxxActiveListView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("foxxListView.ejs"),

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
      $(this.el).html(this.template.render({}));
      _.each(this._subViews, function (v) {
        $("#foxxList").append(v.render());
      });
      this.delegateEvents();
      return this;
    }
  });
}());
