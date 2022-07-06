/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, window, $, _, templateEngine, document, JSONEditor */

(function () {
  'use strict';

  window.ComputedValuesView = Backbone.View.extend({
    el: '#content',
    readOnly: false,
    template: templateEngine.createTemplate('computedValuesView.ejs'),

    initialize: function (options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
      "click #saveComputedValuesButton": 'saveComputedValues'
    },

    render: function () {
      this.breadcrumb();
      arangoHelper.buildCollectionSubNav(this.collectionName, 'Computed Values');
      $(this.el).html(this.template.render({
        parsedVersion: window.versionHelper.toDocuVersion(
          window.frontendConfig.version.version
        )
      }));
      this.renderEditor();
      this.getCollectionProperties();
      this.editor.focus();
    },

    resize: function () {
      $('#computedValuesEditor').height($('.centralRow').height() - 300);
    },

    renderEditor: function () {
      var container = document.getElementById('computedValuesEditor');
      this.resize();
      var options = {
        onChange: function () {
        },
        onModeChange: function (newMode) {
          void (newMode);
        },
        search: true,
        mode: 'code',
        modes: ['tree', 'code'],
        ace: window.ace
      };

      this.editor = new JSONEditor(container, options);
    },

    getCollectionProperties: function () {
      var propCB = (error, data) => {
        if (error) {
          window.arangoHelper.arangoError("Error", "Could not get collection properties");
        } else {
          this.editor.set(data.computedValues || null);
        }
      };
      this.model.getProperties(propCB);
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + _.escape(this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    saveComputedValues: function () {
      var saveCallback = function (error, isCoordinator) {
        void (isCoordinator);
        if (error) {
          arangoHelper.arangoError('Error', 'Could not save computed values definition.');
        } else {
          var newprops = null;
          try {
            newprops = this.editor.get();
          } catch (ex) {
            arangoHelper.arangoError('Error', 'Could not save computed values definition: ' + ex);
            throw ex;
          }

          this.model.changeComputedValues(newprops, (err, data) => {
            if (err) {
              arangoHelper.arangoError('Error', 'Could not save computed values definition for collection ' + this.model.get('name') + ': ' + data.responseJSON.errorMessage);
            } else {
              arangoHelper.arangoNotification('Saved computed values definition for collection ' + this.model.get('name') + '.');
            }
            this.editor.focus();
          });

        } // if error
      }.bind(this);

      window.isCoordinator(saveCallback);
    },

    changeViewToReadOnly: function () {
      window.App.computedValuesView.readOnly = true;
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
      // this method disables all write-based functions
      $('.modal-body input').prop('disabled', 'true');
      $('.modal-body select').prop('disabled', 'true');
      $('.modal-footer button').addClass('disabled');
      $('.modal-footer button').unbind('click');
    }

  });
}());
