/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.ScaleView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("scaleView.ejs"),
    interval: 5000,
    knownServers: [],

    events: {
      "click #addCoord"    : "addCoord",
      "click #removeCoord" : "removeCoord",
      "click #addDBs"      : "addDBs",
      "click #removeDBs"   : "removeDBs"
    },

    setCoordSize: function(number) {
      //TODO AJAX API CALL
      $.ajax({
        type: "POST",
        url: arangoHelper.databaseUrl("/_admin/cluster/setCoordSize"),
        contentType: "application/json",
        data: JSON.stringify({
          value: number
        }),
        success: function() {
          $('#plannedCoords').html(number);
        },
        error: function() {
          arangoHelper.arangoError("Scale", "Could not set coordinator size.");
        }
      });
    },

    setDBsSize: function(number) {
      //TODO AJAX API CALL
      $.ajax({
        type: "POST",
        url: arangoHelper.databaseUrl("/_admin/cluster/setDBsSize"),
        contentType: "application/json",
        data: JSON.stringify({
          value: number
        }),
        success: function() {
          $('#plannedCoords').html(number);
        },
        error: function() {
          arangoHelper.arangoError("Scale", "Could not set coordinator size.");
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
      var value = $(id).html(),
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

    initialize: function (options) {

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash === '#sNodes') {

            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    render: function () {
      var self = this;

      var callback = function() {

        var cb2 = function() {
          self.continueRender();
        }.bind(this);

        this.waitForDBServers(cb2);
      }.bind(this);

      if (!this.initDoneCoords) {
        this.waitForCoordinators(callback);
      }
      else {
        callback();
      }

      window.arangoHelper.buildNodesSubNav('scale');
    },

    continueRender: function() {
      var coords, dbs, self = this;
      coords = this.coordinators.toJSON();
      dbs = this.dbServers.toJSON();

      this.$el.html(this.template.render({
        runningCoords: coords.length,
        runningDBs: dbs.length,
        plannedCoords: undefined,
        plannedDBs: undefined
      }));

      $.ajax({
        type: "GET",
        cache: false,
        url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          self.updateTable(data);
        }
      });
    },

    updateTable: function(data) {
      $('#plannedCoords').html(data.numberOfCoordinators);
      $('#plannedDBs').html(data.numberOfDBServers);

      var scalingActive = '<span class="warning">scaling in progress</span>';
      var scalingDone = '<span class="positive">no scaling process active</span>';

      if (this.coordinators.toJSON().length === data.numberOfCoordinators) {
        $('#statusCoords').html(scalingDone);
      }
      else {
        $('#statusCoords').html(scalingActive);
      }
      
      if (this.dbServers.toJSON().length === data.numberOfDBServers) {
        $('#statusDBs').html(scalingDone);
      }
      else {
        $('#statusDBs').html(scalingActive);
      }
    },

    waitForDBServers: function(callback) {
      var self = this;

      if (this.dbServers.length === 0) {
        window.setInterval(function() {
          self.waitForDBServers(callback);
        }, 300);
      }
      else {
        callback();
      }
    },

    waitForCoordinators: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        }
        else {
          this.initDoneCoords = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());
