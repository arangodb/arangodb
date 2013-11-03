/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true */
/*global require, window, Backbone */

window.Database = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },

  isNew: function() {
    'use strict';
    return false;
  },
  sync: function(method, model, options) {
    'use strict';
    if (method === "update") {
      method = "create";
    }
    return Backbone.sync(method, model, options);
  },

  url: "/_api/database/",

  defaults: {
  }


});
