/*global Backbone */
var app = app || {};

(function () {
	'use strict';


	app.MonitoredCollection = Backbone.Collection.extend({
    url: "list"    
	});
}());
