var app = app || {};

$(function () {
	'use strict';

	app.NaviView = Backbone.View.extend({

    template: new EJS({url: 'templates/naviView.ejs'}),
    
    el: "#navi",

		events: {
			"click li": "navigate"
		},

		initialize: function () {
      var self = this;
      this.collection = new app.MonitoredCollection();
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
		},
    
    navigate: function(event) {
      app.router.navigate("collection/" + $(event.currentTarget).attr("id"), {trigger: true});
    },
    
		// Re-render the titles of the todo item.
		render: function (selection) {
      $(this.el).html(this.template.render({
        collection: this.collection,
        selected: selection
      }));
			return this;
		}
	});
}());
