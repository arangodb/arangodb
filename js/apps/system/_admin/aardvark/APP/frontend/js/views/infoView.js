/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, window, $, frontendConfig */

(function () {
  'use strict';

  window.InfoView = Backbone.View.extend({
    el: '#content',

    initialize: function (options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
    },

    render: function () {
      var self = this;
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Info');

      if (frontendConfig.isCluster) {
        let clusterData = {};
        let callbackShardCount = function (error, data) {
          if (error) {
            arangoHelper.arangoError('Figures', 'Could not get figures.');
            // in error case: try to render the other information
            self.renderInfoView();
          }
          clusterData.shardCounts = data.count;
          self.renderInfoView(clusterData);
        };
        this.model.getShardCounts(callbackShardCount);
      } else {
        this.renderInfoView();
      }

      // check permissions and adjust views
      arangoHelper.checkCollectionPermissions(this.collectionName, this.changeViewToReadOnly);
    },

    changeViewToReadOnly: function () {
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + (this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    renderInfoView: function (cluster) {
      if (this.model.get('locked')) {
        return 0;
      }
      var callbackRev = function (error, revision, figures) {
        if (error) {
          arangoHelper.arangoError('Figures', 'Could not get revision.');
        } else {
          var buttons = [];
          // analyze figures in cluster
          var tableContent = {
            figures: figures,
            revision: revision,
            model: this.model,
            cluster: cluster || {}
          };

          window.modalView.show(
            'modalCollectionInfo.ejs',
            'Collection: ' + (this.model.get('name').length > 64 ? this.model.get('name').substr(0, 64) + "..." : this.model.get('name')),
            buttons,
            tableContent, null, null,
            null, null,
            null, 'content'
          );
        }
      }.bind(this);

      var callback = function (error, data) {
        if (error) {
          arangoHelper.arangoError('Figures', 'Could not get figures.');
        } else {
          var figures = data;
          this.model.getRevision(callbackRev, figures);
        }
      }.bind(this);

      this.model.getFiguresCombined(callback, frontendConfig.isCluster);
    }

  });
}());
