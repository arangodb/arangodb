/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ClusterDownView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterDown.ejs"),

    events: {
      "click #relaunchCluster"  : "relaunchCluster",
      "click #editPlan"         : "editPlan",
      "click #deletePlan"       : "deletePlan"
    },

    initialize : function() {
      this.dbservers = new window.ClusterServers([], {
        interval: this.interval
      });
      this.dbservers.fetch({
        async: false
      });
      this.dbservers.startUpdating();
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    },

    relaunchCluster: function() {
      console.log("relaunchCluster");
      var result = false;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "/cluster/relaunch",
        success: function(data) {
          result = data;
        },
        error: function(data) {
        }
      });

      return result;
    },

    editPlan: function() {
      console.log("editPlan");
      //welches Scenario und entsprechende View rendern
      //siehe showClusterView.js


      var byAddress = this.listByAddress();
      if (Object.keys(byAddress).length === 1) {
        this.type = "testPlan";
      } else {
        this.type = "other";
      }

      if (this.type === "testPlan") {
        window.App.navigate("planTest", {trigger : true});
      } else {
//        window.App.navigate("planSymmetrical", {trigger : true});
        window.App.navigate("planAsymmetrical", {trigger : true});
      }
    },

    deletePlan: function() {
      console.log("deletePlan");
      //irgendwie löschen
      //view planScenario
    },

    listByAddress: function() {
      var byAddress = this.dbservers.byAddress();
      byAddress = this.coordinators.byAddress(byAddress);
      return byAddress;
    }


  });

}());
