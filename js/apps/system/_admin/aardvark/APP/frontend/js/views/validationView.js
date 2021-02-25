/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, window, $, templateEngine, document, JSONEditor */

(function () {
  'use strict';

  window.ValidationView = Backbone.View.extend({
    el: '#content',
    readOnly: false,
    template: templateEngine.createTemplate('validationView.ejs'),

    initialize: function (options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
      "click #saveValidationButton": 'saveValidation'
    },

    render: function () {
      this.breadcrumb();
      arangoHelper.buildCollectionSubNav(this.collectionName, 'Schema');
      $(this.el).html(this.template.render({}));
      this.renderValidationEditor();
      this.getValidationProperties();
      this.editor.focus();
    },

    resize: function () {
      $('#validationEditor').height($('.centralRow').height() - 300);
    },

    renderValidationEditor: function () {
      var container = document.getElementById('validationEditor');
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

    getValidationProperties: function () {
      var propCB = (error, data) => {
        if (error) {
          window.arangoHelper.arangoError("Error", "Could not get collection properties");
        } else {
          this.editor.set(data.schema);
        }
      };
      this.model.getProperties(propCB);
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + (this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    saveValidation: function () {
      var saveCallback = function (error, isCoordinator) {
        void (isCoordinator);
        if (error) {
          arangoHelper.arangoError('Error', 'Could not save schema.');
        } else {
          var newprops = null;
          try {
            newprops = this.editor.get();
          } catch (ex) {
            arangoHelper.arangoError('Error', 'Could not save schema: ' + ex);
            throw ex;
          }

          this.model.changeValidation(newprops, (err, data) => {
            if (err) {
              arangoHelper.arangoError('Error', 'Could not save schema for collection ' + this.model.get('name') + ': ' + data.responseJSON.errorMessage);
            } else {
              arangoHelper.arangoNotification('Saved schema for collection ' + this.model.get('name') + '.');
            }
            this.editor.focus();
          });

        } // if error
      }.bind(this);

      window.isCoordinator(saveCallback);
    },

    changeViewToReadOnly: function () {
      window.App.validationView.readOnly = true;
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
      // this method disables all write-based functions
      $('.modal-body input').prop('disabled', 'true');
      $('.modal-body select').prop('disabled', 'true');
      $('.modal-footer button').addClass('disabled');
      $('.modal-footer button').unbind('click');
    }

  });
}());
