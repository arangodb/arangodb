/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, document, frontendConfig, JSONEditor, Backbone, templateEngine, window, _, localStorage */

(function () {
  'use strict';

  window.ViewView = Backbone.View.extend({
    el: '#content',
    readOnly: false,
    refreshRate: 5000,

    template: templateEngine.createTemplate('viewView.ejs'),

    initialize: function (options) {
      var mode = localStorage.getItem('JSONViewEditorMode');
      if (mode) {
        this.defaultMode = mode;
      }
      this.name = options.name;
    },

    defaultMode: 'tree',

    storeMode: function (mode) {
      var self = this;
      if (mode !== 'view') {
        localStorage.setItem('JSONViewEditorMode', mode);
        self.defaultMode = mode;
        self.editor.setMode(this.defaultMode);
      }
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

    checkIfInProgress: function () {
      if (window.location.hash.search('view/') > -1 && $('.breadcrumb').text().search(this.model.get('name')) > -1) {
        var self = this;

        var callback = function (error, lockedViews) {
          if (error) {
            arangoHelper.arangoError('Views', 'Could not check locked views.');
          } else {
            var found = false;
            _.each(lockedViews, function (foundView) {
              if (self.model.get('name') === foundView.collection) {
                found = true;
              }
            });

            if (found) {
              self.getViewProperties(true);
              self.setInProgress(true);
              window.setTimeout(function () {
                self.checkIfInProgress();
              }, self.refreshRate);
            } else {
              self.getViewProperties(true);
              self.setInProgress(false);
            }
          }
        };

        if (!frontendConfig.ldapEnabled) {
          window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs('view', callback);
        }
      }
    },

    setReadOnlyPermissions: function () {
      this.readOnly = true;
      $('.bottomButtonBar button').attr('disabled', true);
      // editor read only mode
      this.editor.setMode('view');
      $('.jsoneditor-modes').hide();

      // update breadcrumb
      this.breadcrumb(true);
    },

    render: function () {
      this.breadcrumb();
      this.$el.html(this.template.render({}));
      $('#propertiesEditor').height($('.centralRow').height() - 300 + 70 - $('.infoBox').innerHeight() - 10);
      this.initAce();
      this.getViewProperties();
      this.checkIfInProgress();
    },

    jsonContentChanged: function () {
      this.enableSaveButton();
    },

    buttons: [
      '#renameViewButton',
      '#deleteViewButton'
    ],

    disableAllButtons: function () {
      var self = this;
      _.each(this.buttons, function (id) {
        self.disableButton(id);
      });
      this.disableSaveButton();
    },

    enableAllButtons: function () {
      var self = this;
      _.each(this.buttons, function (id) {
        self.enableButton(id);
      });
      this.enableSaveButton();
    },

    enableButton: function (id) {
      $(id).prop('disabled', false);
    },

    disableButton: function (id) {
      $(id).prop('disabled', true);
    },

    enableSaveButton: function () {
      if (!this.readOnly) {
        $('#savePropertiesButton').prop('disabled', false);
        $('#savePropertiesButton').addClass('button-success');
        $('#savePropertiesButton').removeClass('button-close');
      }
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
        onModeChange: function (newMode) {
          self.storeMode(newMode);
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

    getViewProperties: function (expand) {
      var self = this;

      var callback = function (error, data) {
        if (error) {
          arangoHelper.arangoError('View', 'Could not fetch properties for view:' + self.model.get('name'));
        } else {
          delete data.name;
          delete data.code;
          delete data.error;
          self.editor.set(data);
          if (expand) {
            if (self.editor.expandAll) {
              // only valid if tabular view is active
              self.editor.expandAll();
            }
          }
        }
      };

      this.model.getProperties(callback);
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

      var callback = function (error, data, done) {
        if (error) {
          arangoHelper.arangoError('View', 'Could not update view properties.');
        } else {
          if (data) {
            self.editor.set(data);
          }
          if (done) {
            // arangoHelper.arangoNotification('View', 'Saved view properties of: ' + self.model.get('name'));
            self.setInProgress(false);
          } else {
            arangoHelper.arangoNotification('View', 'Saving properties of view: ' + self.model.get('name') + ' in progress.');
            window.setTimeout(function () {
              self.checkIfInProgress();
            }, self.refreshRate);
            self.setInProgress(true);
          }
        }
      };

      this.model.patchProperties(self.editor.get(), callback);
    },

    setInProgress: function (inProgress) {
      if (inProgress) {
        this.disableAllButtons();
        this.editor.setMode('view');
        $('#viewProcessing').show();
        $('#viewDocumentation').hide();
        $('.jsoneditor').attr('style', 'background-color: rgba(0, 0, 0, 0.05) !important');
        $('.jsoneditor-menu button').css('visibility', 'hidden');
        $('.jsoneditor-modes').css('visibility', 'hidden');
      } else {
        this.enableAllButtons();
        this.editor.setMode(this.defaultMode);
        $('.buttonBarInfo').html('');
        $('#viewProcessing').hide();
        $('#viewDocumentation').show();
        $('.jsoneditor').attr('style', 'background-color: rgba(255,255, 255, 1) !important');
        $('.jsoneditor-menu button').css('visibility', 'inline');
        $('.jsoneditor-modes').css('visibility', 'inline');
      }
      arangoHelper.checkDatabasePermissions(this.setReadOnlyPermissions.bind(this));
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

      var callback = function (error, data) {
        if (error) {
          arangoHelper.arangoError('View', 'Could not delete the view.');
        } else {
          window.modalView.hide();
          window.App.navigate('#views', {trigger: true});
        }
      };
      self.model.deleteView(callback);
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
          self.model.set('name', data.name);
          self.breadcrumb();
          window.modalView.hide();
        },
        error: function (error) {
          window.modalView.hide();
          arangoHelper.arangoError('View', error.responseJSON.errorMessage);
        }
      });
    },

    breadcrumb: function (ro) {
      var self = this;
      var title = self.name;
      if (ro) {
        title = title + ' (read-only)';
      }

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'View: ' + arangoHelper.escapeHtml(title)
        );
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
