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
        {success : function(info) {
          console.log("planTestView.js");
          window.App.navigate("showCluster", {trigger: true});
        }}
      );
    },

    render: function() {
      $(this.el).html(this.template.render({
      config : this.model.get("config")
    }));
    }
  });

}());
