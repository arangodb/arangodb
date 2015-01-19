/*global window, Backbone */
(function () {
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
