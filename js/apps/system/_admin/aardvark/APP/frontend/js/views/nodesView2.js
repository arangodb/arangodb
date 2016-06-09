/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodesView2 = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodesView2.ejs"),
    interval: 5000,
    knownServers: [],

    events: {
      "click #nodesContent .pure-table-body .pure-table-row" : "navigateToNode",
      "click #addCoord"    : "addCoord",
      "click #removeCoord" : "removeCoord",
      "click #addDBs"      : "addDBs",
      "click #removeDBs"   : "removeDBs"
    },

    initialize: function () {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        this.updateServerTime();

        //start polling with interval
        this.intervalFunction = window.setInterval(function() {
          if (window.location.hash === '#nodes') {
            self.render();
          }
        }, this.interval);
      }
    },

    navigateToNode: function(elem) {

      if ($(elem.currentTarget).hasClass('noHover')) {
        return;
      }

      var name = $(elem.currentTarget).attr('node');
      window.App.navigate("#node/" + encodeURIComponent(name), {trigger: true});
    },

    render: function () {

      var self = this;

      var scalingFunc = function(nodes) {
        $.ajax({
          type: "GET",
          url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
          contentType: "application/json",
          success: function(data) {
            self.continueRender(nodes, data);
          }
        });
      }.bind(this);

      $.ajax({
        type: "GET",
        cache: false,
        url: arangoHelper.databaseUrl("/_admin/cluster/health"),
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          scalingFunc(data.Health);
        },
        error: function() {
          arangoHelper.arangoError("Cluster", "Could not fetch cluster information");
        }
      });

    },

    continueRender: function(nodes, scaling) {
      var coords = {}, dbs = {}, scale = false;

      _.each(nodes, function(node, name) {
        if (node.Role === 'Coordinator') {
          coords[name] = node;
        }
        else if(node.Role === 'DBServer') {
          dbs[name] = node;
        }
      });

      if (scaling.numberOfDBServers !== null && scaling.numberOfCoordinators !== null) {
        scale = true; 
      }

      this.$el.html(this.template.render({
        coords: coords,
        dbs: dbs,
        scaling: scale,
        plannedDBs: scaling.numberOfDBServers,
        plannedCoords: scaling.numberOfCoordinators
      }));
      
      this.renderCounts(scale); 
    },

    updatePlanned: function(data) {
      if (data.numberOfCoordinators) {
        $('#plannedCoords').val(data.numberOfCoordinators);
        this.renderCounts(true);
      }
      if (data.numberOfDBServers) {
        $('#plannedDBs').val(data.numberOfDBServers);
        this.renderCounts(true);
      }
    },

    setCoordSize: function(number) {
      var self = this;
      var data = {
        numberOfCoordinators: number
      };

      $.ajax({
        type: "PUT",
        url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
        contentType: "application/json",
        data: JSON.stringify(data),
        success: function() {
          self.updatePlanned(data);
        },
        error: function() {
          arangoHelper.arangoError("Scale", "Could not set coordinator size.");
        }
      });
    },

    setDBsSize: function(number) {
      var self = this;
      var data = {
        numberOfDBServers: number
      };

      $.ajax({
        type: "PUT",
        url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
        contentType: "application/json",
        data: JSON.stringify(data),
        success: function() {
          self.updatePlanned(data);
        },
        error: function() {
          arangoHelper.arangoError("Scale", "Could not set coordinator size.");
        }
      });
    },

    renderCounts: function(scale) {
      var self = this;

      if (!scale) {
        $('.title').css('position', 'relative');
        $('.title').css('top', '-4px');
      }

      var renderFunc = function(id, ok, pending, error) {
        var string = '<span class="positive"><span>' + ok + '</span><i class="fa fa-check-circle"></i></span>';
        if (pending && scale === true) {
          string = string + '<span class="warning"><span>' + pending + '</span><i class="fa fa-circle-o-notch fa-spin"></i></span>';
        }
        if (error) {
          string = string + '<span class="negative"><span>' + error + '</span><i class="fa fa-exclamation-circle"></i></span>';
        }
        $(id).html(string);
      }.bind(this);

      var callbackFunction = function(nodes) {
        var coordsErrors = 0, coords = 0, coordsPending = 0,
          dbs = 0, dbsErrors = 0, dbsPending = 0;

        _.each(nodes, function(node) {
          if (node.Role === 'Coordinator') {
            if (node.Status === 'GOOD') {
              coords++;
            }
            else {
              coordsErrors++;
            }
          }
          else if(node.Role === 'DBServer') {
            if (node.Status === 'GOOD') {
              dbs++;
            }
            else {
              dbsErrors++;
            }
          }
        });

        $.ajax({
          type: "GET",
          cache: false,
          url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
          contentType: "application/json",
          processData: false,
          success: function(data) {

            coordsPending = data.numberOfCoordinators - coords;
            dbsPending = data.numberOfDBServers - dbs;

            renderFunc('#infoDBs', dbs, dbsPending, dbsErrors);
            renderFunc('#infoCoords', coords, coordsPending, coordsErrors);
          }
        });

      }.bind(this);

      $.ajax({
        type: "GET",
        cache: false,
        url: arangoHelper.databaseUrl("/_admin/cluster/health"),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callbackFunction(data.Health);
        }
      });
    },

    addCoord: function() {
      this.setCoordSize(this.readNumberFromID('#plannedCoords', true));
    },

    removeCoord: function() {
      this.setCoordSize(this.readNumberFromID('#plannedCoords', false, true));
    },

    addDBs: function() {
      this.setDBsSize(this.readNumberFromID('#plannedDBs', true));
    },

    removeDBs: function() {
      this.setDBsSize(this.readNumberFromID('#plannedDBs', false, true));
    },

    readNumberFromID: function(id, increment, decrement) {
      var value = $(id).val(),
        parsed = false;

        try {
          parsed = JSON.parse(value);
        }
        catch (ignore) {
        }

        if (increment) {
          parsed++;
        }
        if (decrement) {
          if (parsed !== 1) {
            parsed--;
          }
        }

        return parsed;
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());
