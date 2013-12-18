/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,
    
    url: "/_admin/aardvark/foxxes"
  });
}());
