(function() {

  "use strict";

  window.ClusterType = Backbone.Model.extend({

    defaults: {
      "type": "testPlan"
    }
    /*
    url: "/_admin/aardvark/cluster/ClusterType";

    updateUrl: function() {
      this.url = window.getNewRoute("ClusterType");
    }
    */
  });
}());
