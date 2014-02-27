/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine */

(function() {
  "use strict";
  
  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: templateEngine.createTemplate("serverEntry.ejs"),

    events: {
      "click #startSymmetricPlan": "startPlan",
      "click .add": "addEntry",
      "click .delete": "removeEntry"
    },

    startPlan: function() {
      var isDBServer;
      var isCoordinator;
      var self = this;
      var data = {dispatcher : []};
      var foundCoordinator = false;
      var foundDBServer = false;
      $(".dispatcher").each(function(i, dispatcher) {
          var host = $(".host", dispatcher).val();
          var port = $(".port", dispatcher).val();
          if (!host || 0 === host.length || !port || 0 === port.length) {
              return true;
          }
          var hostObject = {host :  host + ":" + port};
          if (!self.isSymmetric) {
              hostObject.isDBServer = !!$(".isDBServer", dispatcher).attr('checked');
              hostObject.isCoordinator = !!$(".isCoordinator", dispatcher).attr('checked');
          }
          foundCoordinator = foundCoordinator || hostObject.isCoordinator;
          foundDBServer = foundDBServer || hostObject.isDBServer;

          data.dispatcher.push(hostObject);
      })
      if (!self.isSymmetric) {
        if (!foundDBServer) {
            alert("Please provide at least one DBServer");
            return;
        }
        if (!foundCoordinator) {
            alert("Please provide at least one Coordinator");
            return;
        }
      } else {
        if ( data.dispatcher.length === 0) {
            alert("Please provide at least one Host");
            return;
        }

      }

      data.type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";
      $.ajax("cluster/plan", {
        type: "POST",
        data: JSON.stringify(data)
      }).done(function(info) {
        window.App.showDownload(info);
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

