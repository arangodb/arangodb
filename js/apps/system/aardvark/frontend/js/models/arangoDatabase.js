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

  url: "/_api/database/",

  methodToURL: {
    'read': '/_api/database/',
    'create': '/_api/database/',
    'update': '/_api/databse/',
    'delete': '/_api/database/' + console.log(this)
  },

  sync: function(method, model, options) {
    options = options || {};
    options.url = model.methodToURL[method.toLowerCase()];

    return Backbone.sync.apply(this, arguments);
  },

  defaults: {
  }


});
