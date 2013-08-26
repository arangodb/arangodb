/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, continue: true, regexp: true */
/*global require, window, Backbone */

window.Foxx = Backbone.Model.extend({
  defaults: {
    "title": "",
    "version": "",
    "mount": "",
    "description": "",
    "git": ""
  },
  
  url: function() {
    'use strict';

    if (this.get("_key")) {
      return "../aardvark/foxxes/" + this.get("_key");
    }
    return "../aardvark/foxxes/install";
  },
  
  isNew: function() {
    'use strict';

    return false;
  }
  
});
