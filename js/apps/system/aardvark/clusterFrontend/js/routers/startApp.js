/*jslint indent: 2, nomen: true, maxlen: 100, white: true, */
/*global window, $, Backbone, document */ 

(function() {
  "use strict";

  $.get("cluster/amIDispatcher", function(data) {
    if (!data) {
      var url = window.location.origin;
      url += window.location.pathname;
      url = url.replace("cluster", "index");
      window.location.replace(url);
    }
  });

  $(document).ready(function() {
    window.App = new window.ClusterRouter();

    Backbone.history.start();

    window.App.navigate("", {trigger: true});

    if(window.App.clusterPlan.get("plan")) {
      if(window.App.clusterPlan.isAlive()) {
        window.App.showCluster();
      } else {
        window.App.handleClusterDown();
      }
    } else {
      window.App.planScenario();
    }

    window.App.handleResize();
  });

}());
