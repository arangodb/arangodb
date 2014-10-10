/*global window, btoa, $, Backbone, templateEngine, alert, _ */

(function() {
  "use strict";

  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("symmetricPlan.ejs"),
    entryTemplate: templateEngine.createTemplate("serverEntry.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    connectionValidationKey: null,

    events: {
      "click #startSymmetricPlan"   : "startPlan",
      "click .add"                  : "addEntry",
      "click .delete"               : "removeEntry",
      "click #cancel"               : "cancel",
      "click #test-all-connections" : "checkAllConnections",
      "focusout .host"              : "checkAllConnections",
      "focusout .port"              : "checkAllConnections",
      "focusout .user"              : "checkAllConnections",
      "focusout .passwd"            : "checkAllConnections"
    },

    cancel: function() {
      if(window.App.clusterPlan.get("plan")) {
        window.App.navigate("handleClusterDown", {trigger: true});
      } else {
        window.App.navigate("planScenario", {trigger: true});
      }
    },

    startPlan: function() {
      var self = this;
      var data = {dispatchers: []};
      var foundCoordinator = false;
      var foundDBServer = false;
            data.useSSLonDBservers = !!$(".useSSLonDBservers").prop('checked');
      data.useSSLonCoordinators = !!$(".useSSLonCoordinators").prop('checked');
      $(".dispatcher").each(function(i, dispatcher) {
        var host = $(".host", dispatcher).val();
        var port = $(".port", dispatcher).val();
        var user = $(".user", dispatcher).val();
        var passwd = $(".passwd", dispatcher).val();
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

        hostObject.username = user;
        hostObject.passwd = passwd;

        foundCoordinator = foundCoordinator || hostObject.isCoordinator;
        foundDBServer = foundDBServer || hostObject.isDBServer;

        data.dispatchers.push(hostObject);
      });
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
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster is being launched');
      delete window.App.clusterPlan._coord;
      window.App.clusterPlan.save(
        data,
        {
          success : function() {
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

    addEntry: function() {
      //disable launch button
      this.disableLaunchButton();

      var lastUser = $("#server_list div.control-group.dispatcher:last .user").val();
      var lastPasswd = $("#server_list div.control-group.dispatcher:last .passwd").val();

      $("#server_list").append(this.entryTemplate.render({
        isSymmetric: this.isSymmetric,
        isFirst: false,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: '',
        user: lastUser,
        passwd: lastPasswd
      }));
    },

    removeEntry: function(e) {
      $(e.currentTarget).closest(".control-group").remove();
      this.checkAllConnections();
    },

    render: function(isSymmetric) {
      var params = {},
        isFirst = true,
        config = window.App.clusterPlan.get("config");
      this.isSymmetric = isSymmetric;
      $(this.el).html(this.template.render({
        isSymmetric : isSymmetric,
        params      : params,
        useSSLonDBservers: config && config.useSSLonDBservers ?
            config.useSSLonDBservers : false,
        useSSLonCoordinators: config && config.useSSLonCoordinators ?
            config.useSSLonCoordinators : false
      }));
      if (config) {
        var self = this,
        isCoordinator = false,
        isDBServer = false;
        _.each(config.dispatchers, function(dispatcher) {
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
          var host = dispatcher.endpoint;
          host = host.split("//")[1];
          host = host.split(":");
          var user = dispatcher.username;
          var passwd = dispatcher.passwd;
          var template = self.entryTemplate.render({
            isSymmetric: isSymmetric,
            isFirst: isFirst,
            host: host[0],
            port: host[1],
            isCoordinator: isCoordinator,
            isDBServer: isDBServer,
            user: user,
            passwd: passwd
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
          port: '',
          user: '',
          passwd: ''
        }));
      }
      //initially disable lunch button
      this.disableLaunchButton();

      $(this.el).append(this.modal.render({}));

    },

    readAllConnections: function() {
      var res = [];
      $(".dispatcher").each(function(key, row) {
        var obj = {
          host: $('.host', row).val(),
          port: $('.port', row).val(),
          user: $('.user', row).val(),
          passwd: $('.passwd', row).val()
        };
        if (obj.host && obj.port) {
          res.push(obj);
        }
      });
      return res;
    },

    checkAllConnections: function() {
      var self = this;
      var connectionValidationKey = Math.random();
      this.connectionValidationKey = connectionValidationKey;
      $('.cluster-connection-check-success').remove();
      $('.cluster-connection-check-fail').remove();
      var list = this.readAllConnections();
      if (list.length) {
        try {
          $.ajax({
            async: true,
            cache: false,
            type: "POST",
            url: "/_admin/aardvark/cluster/communicationCheck",
            data: JSON.stringify(list),
            success: function(checkList) {
              if (connectionValidationKey === self.connectionValidationKey) {
                var dispatcher = $(".dispatcher");
                var i = 0;
                dispatcher.each(function(key, row) {
                  var host = $(".host", row).val();
                  var port = $(".port", row).val();
                  if (host && port) {
                    if (checkList[i]) {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-success">Connection: ok</span>'
                      );
                    } else {
                      $(".controls:first", row).append(
                        '<span class="cluster-connection-check-fail">Connection: fail</span>'
                      );
                    }
                    i++;
                  }
                });
                self.checkDispatcherArray(checkList, connectionValidationKey);
              }
            }
          });
        } catch (e) {
          this.disableLaunchButton();
        }
      }
    },

    checkDispatcherArray: function(dispatcherArray, connectionValidationKey) {
      if(
        (_.every(dispatcherArray, function (e) {return e;}))
          && connectionValidationKey === this.connectionValidationKey
        ) {
        this.enableLaunchButton();
      }
    },

    disableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', 'disabled');
      $('#startSymmetricPlan').removeClass('button-success');
      $('#startSymmetricPlan').addClass('button-neutral');
    },

    enableLaunchButton: function() {
      $('#startSymmetricPlan').attr('disabled', false);
      $('#startSymmetricPlan').removeClass('button-neutral');
      $('#startSymmetricPlan').addClass('button-success');
    }

  });

}());


