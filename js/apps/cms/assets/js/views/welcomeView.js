var app = app || {};

$(function () {
	'use strict';

	app.WelcomeView = Backbone.View.extend({

    template: new EJS({url: 'templates/welcomeView.ejs'}),
    
    el: "#content",

		render: function () {
      $(this.el).html(this.template.text);
			return this;
		}
	});
}());
