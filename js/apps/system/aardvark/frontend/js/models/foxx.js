/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.Foxx = Backbone.Model.extend({
    defaults: {
      "title": "",
      "version": "",
      "mount": "",
      "description": "",
      "git": "",
      "isSystem": false,
      "development": false
    },

    url: function() {
      if (this.get("_key")) {
        return "/_admin/aardvark/foxxes/" + this.get("_key");
      }
      return "/_admin/aardvark/foxxes/install";
    },
    
    isNew: function() {
      return false;
    }
    
  });
}());
