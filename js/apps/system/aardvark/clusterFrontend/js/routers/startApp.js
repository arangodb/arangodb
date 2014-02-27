/*jslint indent: 2, nomen: true, maxlen: 100, white: true, */
/*global window, $, Backbone, document */ 

(function() {
  "use strict";
  $(document).ready(function() {
    window.App = new window.ClusterRouter();
    Backbone.history.start();

    //für zum testen
//      this.clusterPlan.set({"plan": "blub"});

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
