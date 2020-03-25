/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, arangoHelper, Joi, Backbone, window, $ */

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
      "click #saveValidationButton" : 'saveValidation' 
    },

    render: function () {
      // get poperties here?
      this.breadcrumb();
      arangoHelper.buildCollectionSubNav(this.collectionName, 'Validation');
      $(this.el).html(this.template.render({}));
      this.renderValidationEditor();
      this.getValidationProperties();
    },

    resize: function () {
      console.log("resizing");
      $('#validationEditor').height($('.centralRow').height() - 300);
    },

    renderValidationEditor: function () {
      var container = document.getElementById('validationEditor');
      this.resize();
      var options = {
        onChange: function () {
          //self.jsonContentChanged();
        },
        onModeChange: function (newMode) {
          //self.storeMode(newMode);
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
        console.log(error);
        console.log(data);
        if(error) {
          window.arangoHelper.arangoError("Validation", "Could not get collectin properties", null) ;
        } else {
          this.editor.set(data.validation);
        }
      };
      this.model.getProperties(propCB);
    },

    breadcrumb: function () {
      // OBI: do not touch
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + (this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    saveValidation: function () {
      console.log("triggerd save");
      var saveCallback = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not save validation');
        } else {
          var newprops;
          try {
            newprops= this.editor.get();
          } catch (ex) {
            arangoHelper.arangoError('Error', 'Could not save validation' + ex);
            throw ex;
            return;
          }

          this.model.changeValidation(newprops, (err, data) => {
            if(err) {
              arangoHelper.arangoError('Error', 'Could not update validation for' + this.model.get('name') + '.');
            } else {
              arangoHelper.arangoNotification('Saved validaiton for collection ' + this.model.get('name') + '.');
            }
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
