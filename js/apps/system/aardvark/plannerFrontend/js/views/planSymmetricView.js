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
      var isDBServer;
      var isCoordinator;
      var self = this;
      var data = {dbServer : [], coordinator: []};
      $(".dispatcher").each(function(i, dispatcher) {
          var host = $(".host", dispatcher).val();
          var port = $(".port", dispatcher).val();
          if (!host || 0 === host.length || !port || 0 === port.length) {
              return true;
          }
          if (!self.isSymmetric) {
              isDBServer = $(".isDBServer", dispatcher).val();
              isCoordinator = $(".isCoordinator", dispatcher).val();
          } else {
              isDBServer = "true";
              isCoordinator = "true";
          }
          isDBServer === "true" ? data.dbServer.push(host + ":" + port) : null;
          isCoordinator === "true" ? data.coordinator.push(host + ":" + port) : null;
      })
      if (data.dbServer.length === 0) {
        if (!self.isSymmetric) {
            alert("Please provide at least one DBServer");
            return;
        } else {
            alert("Please provide at least one dispatcher");
            return;
        }
      }
      if (data.coordinator.length === 0) {
         alert("Please provide at least one Coordinator");
         return;
      }

      data.type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";
      $.ajax("cluster/plan", {
          type: "POST",
          data: JSON.stringify(data)
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

