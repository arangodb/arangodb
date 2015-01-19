/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCollection = Backbone.Model.extend({
    defaults: {
      "name": "",
      "status": "ok"
    },

    idAttribute: "name",

    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status")
      };
    }
  });
}());

