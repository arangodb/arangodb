/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine */

(function() {
  "use strict";
  
  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: plannerTemplateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: plannerTemplateEngine.createTemplate("serverEntry.ejs"),

    events: {
      "click #startPlan": "startPlan",
      "click .add": "addEntry",
      "click .delete": "removeEntry"
    },

    startPlan: function() {
      var countDispatcher = 0;
      $(".dispatcher").each(function(dispatcher) {
          var host = $(".host", dispatcher).val();
          var port = $(".port", dispatcher).val();
          if (!this.isSymmetric) {
              var isDBServer = $(".isDBServer", dispatcher).val();
              var isCoordinator = $(".isCoordinator", dispatcher).val();
          }
          if (host != null && !host.isEmpty() &&
              port != null && !port.isEmpty() &&
              (this.isSymmetric) || isDBServer || isCoordinator) {
              countDispatcher++;
          }
          
      })
      if (countDispatcher === 0) {
        alert("Please provide at least one dispatcher");
      }
      var type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";









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
      $.ajax("cluster/plan", {
          type: "POST",
          data: {
              type: "testSetup",
              dispatcher: h + ":" + p,
              numberDBServers: parseInt(d, 10),
              numberCoordinators: parseInt(c, 10)
          }
      });
    },

    addEntry: function() {
      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: this.isSymmetric,
        isFirst: false
      }));
    },

    removeEntry: function(e) {
      $(e.currentTarget).closest(".control-group").remove(); 
    },

    render: function(isSymmetric) {
      this.isSymmetric = isSymmetric;
      $(this.el).html(this.template.render({
        isSymmetric : isSymmetric
      }));
      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: isSymmetric,
        isFirst: true
      }));
    }
  });

}());
