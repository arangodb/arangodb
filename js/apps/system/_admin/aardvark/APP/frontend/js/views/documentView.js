/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, localStorage, window, arangoHelper, templateEngine, JSONEditor */
/* global document, _ */

(function () {
  'use strict';

  var createDocumentLink = function (id) {
    var split = id.split('/');
    return 'collection/' +
      encodeURIComponent(split[0]) + '/' +
      encodeURIComponent(split[1]);
  };

  window.DocumentView = Backbone.View.extend({
    el: '#content',
    readOnly: false,
    colid: 0,
    docid: 0,

    customView: false,
    defaultMode: 'tree',

    template: templateEngine.createTemplate('documentView.ejs'),

    events: {
      'click #saveDocumentButton': 'saveDocument',
      'click #deleteDocumentButton': 'deleteDocumentModal',
      'click #confirmDeleteDocument': 'deleteDocument',
      'click #document-from': 'navigateToDocument',
      'click #document-to': 'navigateToDocument',
      'keydown #documentEditor .ace_editor': 'keyPress',
      'keyup .jsoneditor .search input': 'checkSearchBox',
      'click #addDocument': 'addDocument'
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    checkSearchBox: function (e) {
      if ($(e.currentTarget).val() === '') {
        this.editor.expandAll();
      }
    },

    initialize: function () {
      var mode = localStorage.getItem('JSONEditorMode');
      if (mode) {
        this.defaultMode = mode;
      }
    },

    addDocument: function (e) {
      if (!this.readOnly) {
        window.App.documentsView.addDocumentModal(e);
      }
    },

    storeMode: function (mode) {
      var self = this;
      localStorage.setItem('JSONEditorMode', mode);
      self.defaultMode = mode;
      self.editor.setMode(this.defaultMode);
    },

    keyPress: function (e) {
      if (e.ctrlKey && e.keyCode === 13) {
        e.preventDefault();
        this.saveDocument();
      } else if (e.metaKey && e.keyCode === 13) {
        e.preventDefault();
        this.saveDocument();
      }
    },

    editor: 0,

    setType: function () {
      var self = this;
      var callback = function (error, data, type) {
        if (error) {
          arangoHelper.arangoError('Error', 'Document not found.');
          self.renderNotFound(type);
        } else {
          this.type = type;
          this.breadcrumb();
          this.fillInfo();
          this.fillEditor();
          arangoHelper.checkCollectionPermissions(this.colid, this.changeViewToReadOnly.bind(this));
        }
      }.bind(this);

      this.collection.getDocument(this.colid, this.docid, callback);
    },

    renderNotFound: function (id) {
      $('.document-info-div').remove();
      $('.headerButton').remove();
      $('.document-content-div').html(
        '<div class="infoBox errorBox">' +
        '<h4>Error</h4>' +
        '<p>Document not found. Requested ID was: "' + id + '".</p>' +
        '</div>'
      );
    },

    deleteDocumentModal: function () {
      if (!this.readOnly) {
        var buttons = []; var tableContent = [];
        tableContent.push(
          window.modalView.createReadOnlyEntry(
            'doc-delete-button',
            'Confirm delete, document id is',
            this.type._id || this.docid,
            undefined,
            undefined,
            false,
            /[<>&'"]/
          )
        );
        buttons.push(
          window.modalView.createDeleteButton('Delete', this.deleteDocument.bind(this))
        );
        window.modalView.show('modalTable.ejs', 'Delete Document', buttons, tableContent);
      }
    },

    deleteDocument: function () {
      var successFunction = function () {
        if (this.customView) {
          if (this.customDeleteFunction) {
            this.customDeleteFunction();
          }
        } else {
          var navigateTo = 'collection/' + encodeURIComponent(this.colid) + '/documents/1';
          window.modalView.hide();
          window.App.navigate(navigateTo, {trigger: true});
        }
      }.bind(this);

      if (this.type._from && this.type._to) {
        var callbackEdge = function (error) {
          if (error) {
            arangoHelper.arangoError('Edge error', 'Could not delete edge');
          } else {
            successFunction();
          }
        };
        this.collection.deleteEdge(this.colid, this.docid, callbackEdge);
      } else {
        var callbackDoc = function (error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not delete document');
          } else {
            successFunction();
          }
        };
        this.collection.deleteDocument(this.colid, this.docid, callbackDoc);
      }
    },

    navigateToDocument: function (e) {
      var navigateTo = $(e.target).attr('documentLink');
      var test = (navigateTo.split('%').length - 1) % 3;

      if (decodeURIComponent(navigateTo) !== navigateTo && test !== 0) {
        navigateTo = decodeURIComponent(navigateTo);
      }

      if (navigateTo) {
        window.App.navigate(navigateTo, {trigger: true});
      }
    },

    fillInfo: function () {
      var mod = this.collection.first();
      var _id = mod.get('_id');
      var _key = mod.get('_key');
      var _rev = mod.get('_rev');
      var _from = mod.get('_from');
      var _to = mod.get('_to');

      $('#document-type').css('margin-left', '10px');
      $('#document-type').text('_id:');
      $('#document-id').css('margin-left', '0');
      $('#document-id').text(_id);
      $('#document-key').text(_key);
      $('#document-rev').text(_rev);

      if (_from && _to) {
        var hrefFrom = createDocumentLink(_from);
        var hrefTo = createDocumentLink(_to);
        $('#document-from').text(_from);
        $('#document-from').attr('documentLink', hrefFrom);
        $('#document-to').text(_to);
        $('#document-to').attr('documentLink', hrefTo);
      } else {
        $('.edge-info-container').hide();
      }
    },

    fillEditor: function () {
      var toFill = this.removeReadonlyKeys(this.collection.first().attributes);
      $('.disabledBread').last().text(this.collection.first().get('_key'));
      this.editor.set(toFill);
      $('.ace_content').attr('font-size', '11pt');
    },

    jsonContentChanged: function () {
      this.enableSaveButton();
    },

    resize: function () {
      $('#documentEditor').height($('.centralRow').height() - 300);
    },

    render: function () {
      $(this.el).html(this.template.render({}));

      $('#documentEditor').height($('.centralRow').height() - 300);
      this.disableSaveButton();

      var self = this;

      var container = document.getElementById('documentEditor');
      var options = {
        onChange: function () {
          self.jsonContentChanged();
        },
        onModeChange: function (newMode) {
          self.storeMode(newMode);
        },
        search: true,
        mode: 'tree',
        modes: ['tree', 'code'],
        ace: window.ace
        // iconlib: 'fontawesome4'
      };

      this.editor = new JSONEditor(container, options);
      var testMode = localStorage.getItem('JSONEditorMode');
      if (testMode === 'code' || testMode === 'tree') {
        this.defaultMode = testMode;
      }
      if (this.defaultMode) {
        this.editor.setMode(this.defaultMode);
      }

      return this;
    },

    changeViewToReadOnly: function () {
      this.readOnly = true;
      // breadcrumb
      $('.breadcrumb').find('a').html($('.breadcrumb').find('a').html() + ' (read-only)');
      // editor read only mode
      this.editor.setMode('view');
      $('.jsoneditor-modes').hide();
      // bottom buttons
      $('.bottomButtonBar button').addClass('disabled');
      $('.bottomButtonBar button').unbind('click');
      // top add document button
      $('#addDocument').addClass('disabled');
    },

    cleanupEditor: function () {
      if (this.editor) {
        this.editor.destroy();
      }
    },

    removeReadonlyKeys: function (object) {
      return _.omit(object, ['_key', '_id', '_from', '_to', '_rev']);
    },

    saveDocument: function () {
      if ($('#saveDocumentButton').attr('disabled') === undefined) {
        if (this.collection.first().attributes._id.substr(0, 1) === '_') {
          var buttons = []; var tableContent = [];
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'doc-save-system-button',
              'Caution',
              'You are modifying a system collection. Really continue?',
              undefined,
              undefined,
              false,
              /[<>&'"]/
            )
          );
          buttons.push(
            window.modalView.createSuccessButton('Save', this.confirmSaveDocument.bind(this))
          );
          window.modalView.show('modalTable.ejs', 'Modify System Collection', buttons, tableContent);
        } else {
          this.confirmSaveDocument();
        }
      }
    },

    confirmSaveDocument: function () {
      var self = this;
      window.modalView.hide();

      var model;

      try {
        model = this.editor.get();
      } catch (e) {
        this.errorConfirmation(e);
        this.disableSaveButton();
        return;
      }

      model = JSON.stringify(model);

      if (this.type === 'edge' || this.type._from) {
        var callbackE = function (error, data) {
          if (error) {
            arangoHelper.arangoError('Error', data.responseJSON.errorMessage);
          } else {
            this.successConfirmation();
            this.disableSaveButton();

            if (self.customView) {
              if (self.customSaveFunction) {
                self.customSaveFunction(data);
              }
            }
          }
        }.bind(this);

        this.collection.saveEdge(this.colid, this.docid, $('#document-from').html(), $('#document-to').html(), model, callbackE);
      } else {
        var callback = function (error, data) {
          if (error || (data[0] && data[0].error)) {
            if (data[0] && data[0].error) {
              arangoHelper.arangoError('Error', data[0].errorMessage);
            } else {
              arangoHelper.arangoError('Error', data.responseJSON.errorMessage);
            }
          } else {
            this.successConfirmation();
            this.disableSaveButton();

            if (self.customView) {
              if (self.customSaveFunction) {
                self.customSaveFunction(data);
              }
            }
          }
        }.bind(this);

        this.collection.saveDocument(this.colid, this.docid, model, callback);
      }
    },

    successConfirmation: function () {
      arangoHelper.arangoNotification('Document saved.');
    },

    errorConfirmation: function (e) {
      arangoHelper.arangoError('Document editor: ', e);
    },

    enableSaveButton: function () {
      $('#saveDocumentButton').prop('disabled', false);
      $('#saveDocumentButton').addClass('button-success');
      $('#saveDocumentButton').removeClass('button-close');
    },

    disableSaveButton: function () {
      $('#saveDocumentButton').prop('disabled', true);
      $('#saveDocumentButton').addClass('button-close');
      $('#saveDocumentButton').removeClass('button-success');
    },

    breadcrumb: function () {
      var name = window.location.hash.split('/');
      $('#subNavigationBar .breadcrumb').html(
        '<a href="#collection/' + name[1] + '/documents/1">Collection: ' + name[1] + '</a>' +
        '<i class="fa fa-chevron-right"></i>' +
        this.type.charAt(0).toUpperCase() + this.type.slice(1) + ': ' + name[2]
      );
    },

    escaped: function (value) {
      return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    }
  });
}());
