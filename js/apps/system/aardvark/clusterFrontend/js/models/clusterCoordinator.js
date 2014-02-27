/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCoordinator = Backbone.Model.extend({

    defaults: {
      "name": "",
      "url": "",
      "status": "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/Coordinators";

    updateUrl: function() {
      this.url = window.getNewRoute("Coordinators");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status"),
        url: this.get("url")
      };
    }
    
  });
}());
