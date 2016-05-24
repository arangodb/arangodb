/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodesView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodesView.ejs"),
    interval: 5000,
    knownServers: [],

    events: {
      "click #nodesContent .pure-table-body .pure-table-row" : "navigateToNode"
    },

    initialize: function (options) {

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();
        this.toRender = options.toRender;

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash === '#cNodes' || window.location.hash === '#dNodes') {

            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    navigateToNode: function(elem) {

      if (window.location.hash === '#dNodes') {
        return;
      }

      var name = $(elem.currentTarget).attr('node');
      window.App.navigate("#node/" + encodeURIComponent(name), {trigger: true});
    },

    render: function () {

      var callback = function() {
        this.continueRender();
      }.bind(this);

      if (!this.initDoneCoords) {
        this.waitForCoordinators(callback);
      }
      else {
        callback();
      }
    },

    continueRender: function() {
      var coords;


      if (this.toRender === 'coordinator') {
        coords = this.coordinators.toJSON();
      }
      else {
        coords = this.dbServers.toJSON();
      }

      this.$el.html(this.template.render({
        coords: coords,
        type: this.toRender
      }));

      window.arangoHelper.buildNodesSubNav(this.toRender);
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
