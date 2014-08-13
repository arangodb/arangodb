/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true */
/*global require, window, Backbone, $, arangoHelper */
(function() {
  'use strict';
  window.ArangoQuery = Backbone.Model.extend({

    urlRoot: "/_api/user",

    defaults: {
      name: "",
      type: "custom",
      value: ""
    }

  });
}());
