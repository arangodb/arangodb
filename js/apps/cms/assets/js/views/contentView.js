var app = app || {};

$(function () {
	'use strict';

	app.ContentView = Backbone.View.extend({

    template: new EJS({url: 'templates/contentView.ejs'}),
    
    el: "#content",

		events: {
      "click .delete": "deleteDocument",
      "click #addNew": "addDocument",
      "click .edit": "editDocument"
		},

		initialize: function () {
      var self = this;
      this.randomNumber = Math.random();
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
      this.collection.bind("change", self.render.bind(self));
      this.collection.bind("remove", self.render.bind(self));
		},

    deleteDocument: function(event) {
      var _id = $(event.currentTarget).closest("tr").attr("id");
      this.collection.get(_id).destroy();
    },

    addDocument: function() {
      var cols = this.collection.getColumns(),
        modal = new app.NewView({collection: this.collection, columns: cols});
      modal.render();
    },
    
    editDocument: function(event) {
      
      var _id = $(event.currentTarget).closest("tr").attr("id"),
        doc = this.collection.get(_id),
        cols = this.collection.getColumns(),
        modal = new app.EditView({model: doc, columns: cols});
      modal.render();
    },

		// Re-render the titles of the todo item.
		render: function () {
      $(this.el).off();
      $(this.el).html(this.template.render({collection: this.collection}));
      this.delegateEvents();
			return this;
		}
	});
}());

