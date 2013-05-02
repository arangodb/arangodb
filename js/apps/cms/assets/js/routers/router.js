/*global Backbone */
var app = app || {};

(function () {
	'use strict';

	// Todo Router
	// ----------
	var CMSRouter = Backbone.Router.extend({
    initialize: function() {
      this.navi = new app.NaviView();
      this.factory = new app.ContentCollectionFactory();
      this.welcome = new app.WelcomeView();
    },
    
		routes: {
			"collection/:name": "displayCollection",
      "": "displayWelcome"
		},

    displayWelcome: function () {
      this.navi.render();
      this.welcome.render();
    },

		displayCollection: function (name) {
      this.navi.render(name);
      this.factory.create(name, function(res) {
        var view = new app.ContentView({collection: res});
        view.render();
      });
		}
	});

	app.router = new CMSRouter();
	Backbone.history.start();
})();
