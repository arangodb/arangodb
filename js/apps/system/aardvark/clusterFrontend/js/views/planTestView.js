/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.PlanTestView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("testPlan.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #startTestPlan": "startPlan",
      "click #cancel": "cancel"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      var h = $("#host").val(),
        p = $("#port").val(),
        c = $("#coordinators").val(),
        d = $("#dbs").val();
      if (!h) {
        alert("Please define a host");
        return;
      }
      if (!p) {
        alert("Please define a port");
        return;
      }
      if (!c || c < 0) {
        alert("Please define a number of coordinators");
        return;
      }
      if (!d || d < 0) {
        alert("Please define a number of database servers");
        return;
      }
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        {
          type: "testSetup",
          dispatchers: h + ":" + p,
          numberDBServers: parseInt(d, 10),
          numberCoordinators: parseInt(c, 10)
        },
        {
          success: function(info) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            window.App.updateAllUrls();
            window.App.navigate("showCluster", {trigger: true});
          },
          error: function(obj, err) {
            $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
            $('#waitModalLayer').modal('hide');
            alert("Error while starting the cluster: " + err.statusText);
          }
        }
      );
    },

    render: function() {
      var param = {};
      var config = window.App.clusterPlan.get("config");
      if (config) {
        param.dbs = config.numberOfDBservers;
        param.coords = config.numberOfCoordinators;
        var host = config.dispatchers.d1.endpoint;
        host = host.split("://")[1];
        host = host.split(":");
        param.hostname = host[0];
        param.port = host[1];
      } else {
        param.dbs = 3;
        param.coords = 2;
        param.hostname = window.location.hostname;
        param.port = window.location.port; 
      }
      $(this.el).html(this.template.render(param));
      $(this.el).append(this.modal.render({}));
    }
  });

}());
