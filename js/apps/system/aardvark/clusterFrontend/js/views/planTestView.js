/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.PlanTestView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("testPlan.ejs"),

    events: {
      "click #startTestPlan": "startPlan"
    },

    startPlan: function() {
      var h = $("#host").val(),
        p = $("#port").val(),
        c = $("#coordinators").val(),
        d = $("#dbs").val();
      if (!h) {
        alert("Please define a Host");
        return;
      }
      if (!p) {
        alert("Please define a Port");
        return;
      }
      if (!c) {
        alert("Please define a number of Coordinators");
        return;
      }
      if (!d) {
        alert("Please define a number of DBServers");
        return;
      }
      this.model.save(
        {
          type: "testSetup",
          dispatcher: h + ":" + p,
          numberDBServers: parseInt(d, 10),
          numberCoordinators: parseInt(c, 10)
        },
        {
          success : function(info) {
            window.App.navigate("showCluster", {trigger: true});
          }
        }
      );
    },

    render: function() {
      var param = {};
      var config = this.model.get("config");
      if (config) {
        param.dbs = config.numberOfDBservers;
        param.coords = config.numberOfCoordinators;
        var host = config.dispatchers.d1.endpoint;
        host = host.split("://")[1];
        host = host.split(":");
        param.hostname = host[0];
        param.port = host[1];
      } else {
        param.dbs = 2;
        param.coords = 1;
        param.hostname = window.location.hostname;
        param.port = window.location.port; 
      }
      $(this.el).html(this.template.render(param));
    }
  });

}());
