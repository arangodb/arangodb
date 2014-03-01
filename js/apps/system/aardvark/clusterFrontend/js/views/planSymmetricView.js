/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine */

(function() {
  "use strict";
  
  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: templateEngine.createTemplate("serverEntry.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #startSymmetricPlan": "startPlan",
      "click .add": "addEntry",
      "click .delete": "removeEntry",
      "click #cancel": "cancel"
    },

    cancel: function() {
      window.App.navigate("", {trigger: true});
    },

    startPlan: function() {
      var isDBServer;
      var isCoordinator;
      var self = this;
      var data = {dispatchers: []};
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
              hostObject.isDBServer = !!$(".isDBServer", dispatcher).prop('checked');
              hostObject.isCoordinator = !!$(".isCoordinator", dispatcher).prop('checked');
          } else {
            hostObject.isDBServer = true;
            hostObject.isCoordinator = true;
          }

          foundCoordinator = foundCoordinator || hostObject.isCoordinator;
          foundDBServer = foundDBServer || hostObject.isDBServer;

          data.dispatchers.push(hostObject);
      })
      if (!self.isSymmetric) {
        if (!foundDBServer) {
            alert("Please provide at least one database server");
            return;
        }
        if (!foundCoordinator) {
            alert("Please provide at least one coordinator");
            return;
        }
      } else {
        if ( data.dispatchers.length === 0) {
            alert("Please provide at least one host");
            return;
        }

      }

      data.type = this.isSymmetric ? "symmetricalSetup" : "asymmetricalSetup";
      $('#waitModalLayer').modal('show');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      this.model.save(
        data,
        {
          success : function(info) {
            $('#waitModalLayer').modal('hide');
            window.App.navigate("showCluster", {trigger: true});
          }
        }
      );
    },

    addEntry: function() {
      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: this.isSymmetric,
        isFirst: false,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: ''
      }));
    },

    removeEntry: function(e) {
      $(e.currentTarget).closest(".control-group").remove();
    },

    render: function(isSymmetric) {
      this.isSymmetric = isSymmetric;
      $(this.el).html(this.template.render({
        isSymmetric : isSymmetric,
        params      : params
      }));
      var params = {},
        isFirst = true,
        config = this.model.get("config");
      if (config) {
        var self = this,
        isCoordinator = false,
        isDBServer = false;
        _.each(config.dispatchers, function(dispatcher, key) {
          if (dispatcher.allowDBservers === undefined) {
            isDBServer = true;
          } else {
            isDBServer = dispatcher.allowDBservers;
          }
          if (dispatcher.allowCoordinators === undefined) {
            isCoordinator = true;
          } else {
            isCoordinator = dispatcher.allowCoordinators;
          }
          var host = dispatcher.endpoint,
            ip,
            port;
          host = host.split("//")[1];
          host = host.split(":");
          var template = self.entryTemplate.render({
            isSymmetric: isSymmetric,
            isFirst: isFirst,
            host: host[0],
            port: host[1],
            isCoordinator: isCoordinator,
            isDBServer: isDBServer
          });
          $("#server_list").append(template);
          isFirst = false;
        });
      } else {
        $("#server_list").append(this.entryTemplate.render({
          isSymmetric: isSymmetric,
          isFirst: true,
          isCoordinator: true,
          isDBServer: true,
          host: '',
          port: ''
        }));
      }
      $(this.el).append(this.modal.render({}));

    }
  });

}());

