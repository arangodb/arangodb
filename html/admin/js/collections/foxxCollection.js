/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, Backbone */
window.FoxxCollection = Backbone.Collection.extend({
  model: window.Foxx,
  
  url: "../aardvark/foxxes"
});
