/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, Joi, frontendConfig */

(function () {
  'use strict';

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    initialize: function (options) {
      this.options = options;
    },

    loadGraphViewer: function (graphName, refetch) {
      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('', '');
        } else {
          var edgeDefs = this.collection.get(graphName).get('edgeDefinitions');
          if (!edgeDefs || edgeDefs.length === 0) {
            // User Info
            return;
          }
          var adapterConfig = {
            type: 'gharial',
            graphName: graphName,
            baseUrl: arangoHelper.databaseUrl('/')
          };
          var width = $('#content').width() - 75;
          $('#content').html('');

          var height = arangoHelper.calculateCenterDivHeight();

          this.ui = new GraphViewerUI($('#content')[0], adapterConfig, width, $('.centralRow').height() - 135, {
            nodeShaper: {
              label: '_key',
              color: {
                type: 'attribute',
                key: '_key'
              }
            }

          }, true);

          $('.contentDiv').height(height);
        }
      }.bind(this);

      if (refetch) {
        this.collection.fetch({
          cache: false,
          success: function () {
            callback();
          }
        });
      } else {
        callback();
      }
    },

    render: function (name, refetch) {
      if (name) {
        this.loadGraphViewer(name, refetch);
      }
      return this;
    },
  });
}());
