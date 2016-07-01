/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, sigma, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.GraphViewer2 = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('graphViewer2.ejs'),

    initialize: function (options) {
      this.name = options.name;
    },

    render: function () {
      this.$el.html(this.template.render({}));

      // adjust container widht + height
      $('#graph-container').width($('.centralContent').width());
      $('#graph-container').height($('.centralRow').height() - 150);

      this.fetchGraph();
    },

    fetchGraph: function () {
      var self = this;

      $.ajax({
        type: 'GET',
        url: arangoHelper.databaseUrl('/_admin/aardvark/graph/' + encodeURIComponent(this.name)),
        contentType: 'application/json',
        success: function (data) {
          self.renderGraph(data);
        }
      });
    },

    renderGraph: function (graph) {
      var s;

      if (graph.edges.left === 0) {
        return;
      }

      console.log(graph);

      s = new sigma({
        graph: graph,
        container: 'graph-container'
      });

      // Configure the noverlap layout:
      var noverlapListener = s.configNoverlap({
        nodeMargin: 0.1,
        scaleNodes: 1.05,
        gridSize: 75,
        easing: 'quadraticInOut', // animation transition function
        duration: 10000 // animation duration. Long here for the purposes of this example only
      });

      // Bind the events:
      noverlapListener.bind('start stop interpolate', function (e) {
        console.log(e.type);
        if (e.type === 'start') {
          console.time('noverlap');
        }
        if (e.type === 'interpolate') {
          console.timeEnd('noverlap');
        }
      });

      // Start the layout:
      s.startNoverlap();
    }

  });
}());
