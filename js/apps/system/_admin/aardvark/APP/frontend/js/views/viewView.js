/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, JSONEditor, Backbone, templateEngine, window */
(function () {
  'use strict';

  window.ViewView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('viewView.ejs'),

    initialize: function (options) {
      this.name = options.name;
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #savePropertiesButton': 'patchView',
      'click #renameViewButton': 'renameView',
      'click #deleteViewButton': 'deleteView'
    },

    render: function () {
      this.breadcrumb();
      this.$el.html(this.template.render({}));
      $('#propertiesEditor').height($('.centralRow').height() - 300 + 70);
      this.initAce();
      this.getView();
    },

    jsonContentChanged: function () {
      this.enableSaveButton();
    },

    enableSaveButton: function () {
      $('#savePropertiesButton').prop('disabled', false);
      $('#savePropertiesButton').addClass('button-success');
      $('#savePropertiesButton').removeClass('button-close');
    },

    disableSaveButton: function () {
      $('#savePropertiesButton').prop('disabled', true);
      $('#savePropertiesButton').addClass('button-close');
      $('#savePropertiesButton').removeClass('button-success');
    },

    initAce: function (properties) {
      var self = this;
      var container = document.getElementById('propertiesEditor');
      var options = {
        onChange: function () {
          self.jsonContentChanged();
        },
        search: true,
        mode: 'code',
        modes: ['tree', 'code'],
        ace: window.ace
      };

      this.editor = new JSONEditor(container, options);
      if (this.defaultMode) {
        this.editor.setMode(this.defaultMode);
      }
    },

    getView: function () {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(self.name) + '/properties'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          delete data.name;
          delete data.code;
          delete data.error;
          self.editor.set(data);
        },
        error: function (error) {
          arangoHelper.arangoError('View', error.errorMessage);
        }
      });
    },

    patchView: function () {
      var self = this;
      var data;

      try {
        data = self.editor.get();
      } catch (e) {
        arangoHelper.arangoError('View', e);
      }

      if (!data) {
        return;
      }

      $.ajax({
        type: 'PATCH',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(self.name) + '/properties'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify(self.editor.get()),
        success: function (properties) {
          self.disableSaveButton();
          self.editor.set(properties);
          arangoHelper.arangoNotification('View', 'Saved properties.');
        },
        error: function (error) {
          arangoHelper.arangoError('View', error.responseJSON.errorMessage);
        }
      });
    },

    deleteView: function () {
      var buttons = []; var tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'view-delete-button',
          'Delete',
          'Really delete View?',
          undefined,
          undefined,
          false,
          /[<>&'"]/
        )
      );
      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteViewTrue.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Delete View', buttons, tableContent);
    },

    deleteViewTrue: function () {
      var self = this;
      $.ajax({
        type: 'DELETE',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(self.name)),
        contentType: 'application/json',
        processData: false,
        success: function () {
          window.modalView.hide();
          window.App.navigate('#views', {trigger: true});
        },
        error: function (error) {
          window.modalView.hide();
          arangoHelper.arangoError('View', error.responseJSON.errorMessage);
        }
      });
    },

    renameView: function () {
      var self = this;
      var buttons = []; var tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry('editCurrentName', 'Name', self.name, false, 'Name', false)
      );
      buttons.push(
        window.modalView.createSuccessButton('Rename', this.renameViewTrue.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Rename View', buttons, tableContent);
    },

    renameViewTrue: function () {
      var self = this;

      var name = $('#editCurrentName').val();
      if (!name) {
        arangoHelper.arangoError('View', 'Please fill in a view name.');
        return;
      }

      $.ajax({
        type: 'PUT',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(self.name) + '/rename'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify({
          name: $('#editCurrentName').val()
        }),
        success: function (data) {
          self.name = data.name;
          self.breadcrumb();
          window.modalView.hide();
        },
        error: function (error) {
          window.modalView.hide();
          arangoHelper.arangoError('View', error.responseJSON.errorMessage);
        }
      });
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'View: ' + this.name
        );
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
