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
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Info');

      this.renderInfoView();
      // check permissions and adjust views
      arangoHelper.checkCollectionPermissions(this.collectionName, this.changeViewToReadOnly);
    },

    changeViewToReadOnly: function () {
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    },

    renderInfoView: function () {
      if (this.model.get('locked')) {
        return 0;
      }
      var callbackRev = function (error, revision, figures) {
        if (error) {
          arangoHelper.arangoError('Figures', 'Could not get revision.');
        } else {
          var buttons = [];
          // analyse figures in cluster
          if (frontendConfig.isCluster) {
            if (figures.figures.alive.size === 0 &&
              figures.figures.alive.count === 0 &&
              figures.figures.datafiles.count === 0 &&
              figures.figures.datafiles.fileSize === 0 &&
              figures.figures.journals.count === 0 &&
              figures.figures.journals.fileSize === 0 &&
              figures.figures.compactors.count === 0 &&
              figures.figures.compactors.fileSize === 0 &&
              figures.figures.dead.size === 0 &&
              figures.figures.dead.count === 0) {
              figures.walMessage = ' - not ready yet - ';
            }
          }
          var tableContent = {
            figures: figures,
            revision: revision,
            model: this.model
          };
          window.modalView.show(
            'modalCollectionInfo.ejs',
            'Collection: ' + this.model.get('name'),
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

      this.model.getFigures(callback);
    }

  });
}());
