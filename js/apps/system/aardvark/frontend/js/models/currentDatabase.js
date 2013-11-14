/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, window*/

(function() {
  "use strict";

  window.CurrentDatabase = Backbone.Model.extend({

    url: "/_api/database/current"
      
  });
}());
