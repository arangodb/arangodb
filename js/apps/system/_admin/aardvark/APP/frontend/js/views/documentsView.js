/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, _, $, window, arangoHelper, templateEngine, Joi, btoa */
/* global numeral */

(function () {
  'use strict';
  window.DocumentsView = window.PaginationView.extend({
    filters: { '0': true },
    filterId: 0,
    paginationDiv: '#documentsToolbarF',
    idPrefix: 'documents',

    addDocumentSwitch: true,
    activeFilter: false,
    lastCollectionName: undefined,
    restoredFilters: [],

    editMode: false,

    allowUpload: false,

    el: '#content',
    table: '#documentsTableID',

    template: templateEngine.createTemplate('documentsView.ejs'),

    collectionContext: {
      prev: null,
      next: null
    },

    unbindEvents: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
    },

    editButtons: ['#deleteSelected', '#moveSelected'],

    initialize: function (options) {
      this.documentStore = options.documentStore;
      this.collectionsStore = options.collectionsStore;
      this.tableView = new window.TableView({
        el: this.table,
        collection: this.collection
      });
      this.tableView.setRowClick(this.clicked.bind(this));
      this.tableView.setRemoveClick(this.remove.bind(this));
    },

    resize: function () {
      var dropdownVisible = false;
      _.each($('.documentsDropdown').first().children(), function (elem) {
        if ($(elem).is(':visible')) {
          dropdownVisible = true;
        }
      });
      if (dropdownVisible) {
        $('#docPureTable').height($('.centralRow').height() - 210 - 57);
        $('#docPureTable .pure-table-body').css('max-height', $('#docPureTable').height() - 47);
      } else {
        $('#docPureTable').height($('.centralRow').height() - 210);
        $('#docPureTable .pure-table-body').css('max-height', $('#docPureTable').height() - 47);
      }
    },

    setCollectionId: function (colid, page) {
      this.collection.setCollection(colid);
      this.collection.setPage(page);
      this.page = page;

      var callback = function (error, type) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not get collection properties.');
        } else {
          this.type = type;
          this.collection.getDocuments(this.getDocsCallback.bind(this));
          this.collectionModel = this.collectionsStore.get(colid);
        }
      }.bind(this);

      arangoHelper.collectionApiType(colid, null, callback);
    },

    getDocsCallback: function (error) {
      // Hide first/last pagination
      $('#documents_last').css('visibility', 'hidden');
      $('#documents_first').css('visibility', 'hidden');

      if (error) {
        window.progressView.hide();
        arangoHelper.arangoError('Document error', 'Could not fetch requested documents.');
      } else if (!error || error !== undefined) {
        window.progressView.hide();
        this.drawTable();
        this.renderPaginationElements();
        // check permissions and adjust views
        arangoHelper.checkCollectionPermissions(this.collection.collectionID, this.changeViewToReadOnly);
      }
    },

    events: {
      'click #collectionPrev': 'prevCollection',
      'click #collectionNext': 'nextCollection',
      'click #filterCollection': 'filterCollection',
      'click #markDocuments': 'editDocuments',
      'click #importCollection': 'importCollection',
      'click #exportCollection': 'exportCollection',
      'click #filterSend': 'sendFilter',
      'click #addFilterItem': 'addFilterItem',
      'click .removeFilterItem': 'removeFilterItem',
      'click #deleteSelected': 'deleteSelectedDocs',
      'click #moveSelected': 'moveSelectedDocs',
      'click #addDocumentButton': 'addDocumentModal',
      'click #documents_first': 'firstDocuments',
      'click #documents_last': 'lastDocuments',
      'click #documents_prev': 'prevDocuments',
      'click #documents_next': 'nextDocuments',
      'click #confirmDeleteBtn': 'confirmDelete',
      'click .key': 'nop',
      'keyup': 'returnPressedHandler',
      'keydown .queryline input': 'filterValueKeydown',
      'click #importModal': 'showImportModal',
      'click #resetView': 'resetView',
      'click #confirmDocImport': 'startUpload',
      'click #exportDocuments': 'startDownload',
      'change #documentSize': 'setPagesize',
      'change #docsSort': 'setSorting'
    },

    showSpinner: function () {
      $('.upload-indicator').show();
    },

    hideSpinner: function () {
      $('.upload-indicator').hide();
    },

    showImportModal: function () {
      $('#docImportModal').modal('show');
    },

    hideImportModal: function () {
      $('#docImportModal').modal('hide');
    },

    setPagesize: function () {
      var size = $('#documentSize').find(':selected').val();
      this.collection.setPagesize(size);
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    setSorting: function () {
      var sortAttribute = $('#docsSort').val();

      if (sortAttribute === '' || sortAttribute === undefined || sortAttribute === null) {
        sortAttribute = '_key';
      }

      this.collection.setSort(sortAttribute);
    },

    returnPressedHandler: function (event) {
      if (event.keyCode === 13 && $(event.target).is($('#docsSort'))) {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
      }
      if (event.keyCode === 13) {
        if ($('#confirmDeleteBtn').attr('disabled') === false) {
          this.confirmDelete();
        }
      }
    },

    nop: function (event) {
      event.stopPropagation();
    },

    resetView: function () {
      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('Document', 'Could not fetch documents count');
        }
      };

      // clear all input/select - fields
      $('input').val('');
      $('select').val('==');
      this.removeAllFilterItems();
      $('#documentSize').val(this.collection.getPageSize());

      $('#documents_last').css('visibility', 'visible');
      $('#documents_first').css('visibility', 'visible');
      this.addDocumentSwitch = true;
      this.collection.resetFilter();
      this.collection.loadTotal(callback);
      this.restoredFilters = [];

      // for resetting json upload
      this.allowUpload = false;
      this.files = undefined;
      this.file = undefined;
      $('#confirmDocImport').attr('disabled', true);

      this.markFilterToggle();
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    startDownload: function () {
      var query = this.collection.buildDownloadDocumentQuery();

      if (query !== '' || query !== undefined || query !== null) {
        var url = 'query/result/download/' + btoa(JSON.stringify(query));
        arangoHelper.download(url);
      } else {
        arangoHelper.arangoError('Document error', 'could not download documents');
      }
    },

    startUpload: function () {
      var callback = function (error, msg) {
        if (error) {
          arangoHelper.arangoError('Upload', msg);
        } else {
          this.hideImportModal();
          this.resetView();
        }
        this.hideSpinner();
      }.bind(this);

      if (this.allowUpload === true) {
        this.showSpinner();
        this.collection.uploadDocuments(this.file, callback);
      }
    },

    uploadSetup: function () {
      var self = this;
      $('#importDocuments').change(function (e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];
        $('#confirmDocImport').attr('disabled', false);

        self.allowUpload = true;
      });
    },

    buildCollectionLink: function (collection) {
      return 'collection/' + encodeURIComponent(collection.get('name')) + '/documents/1';
    },

    markFilterToggle: function () {
      if (this.restoredFilters.length > 0) {
        $('#filterCollection').addClass('activated');
      } else {
        $('#filterCollection').removeClass('activated');
      }
    },

    // need to make following functions automatically!

    editDocuments: function (e) {
      if (!$(e.currentTarget).hasClass('disabled')) {
        $('#importCollection').removeClass('activated');
        $('#exportCollection').removeClass('activated');
        this.markFilterToggle();
        $('#markDocuments').toggleClass('activated');
        this.changeEditMode();
        $('#filterHeader').hide();
        $('#importHeader').hide();
        $('#editHeader').slideToggle(1);
        $('#exportHeader').hide();

        var self = this;
        window.setTimeout(function () {
          self.resize();
        }, 50);
      }
    },

    filterCollection: function () {
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      this.markFilterToggle();
      this.activeFilter = true;
      $('#importHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
      $('#filterHeader').slideToggle(1);

      var self = this;
      window.setTimeout(function () {
        self.resize();
      }, 50);

      var i;
      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          $('#attribute_name' + i).focus();
          return;
        }
      }
    },

    exportCollection: function () {
      $('#importCollection').removeClass('activated');
      $('#filterHeader').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#exportCollection').toggleClass('activated');
      this.markFilterToggle();
      $('#exportHeader').slideToggle(1);
      $('#importHeader').hide();
      $('#filterHeader').hide();
      $('#editHeader').hide();
      var self = this;
      window.setTimeout(function () {
        self.resize();
      }, 50);
    },

    importCollection: function (e) {
      if (!$(e.currentTarget).hasClass('disabled')) {
        this.markFilterToggle();
        $('#markDocuments').removeClass('activated');
        this.changeEditMode(false);
        $('#importCollection').toggleClass('activated');
        $('#exportCollection').removeClass('activated');
        $('#importHeader').slideToggle(1);
        $('#filterHeader').hide();
        $('#editHeader').hide();
        $('#exportHeader').hide();
        var self = this;
        window.setTimeout(function () {
          self.resize();
        }, 50);
      }
    },

    changeEditMode: function (enable) {
      if (enable === false || this.editMode === true) {
        $('#docPureTable .pure-table-body .pure-table-row').css('cursor', 'default');
        $('.deleteButton').fadeIn();
        $('.addButton').fadeIn();
        $('.selected-row').removeClass('selected-row');
        this.editMode = false;
        this.tableView.setRowClick(this.clicked.bind(this));
      } else {
        $('#docPureTable .pure-table-body .pure-table-row').css('cursor', 'copy');
        $('.deleteButton').fadeOut();
        $('.addButton').fadeOut();
        $('.selectedCount').text(0);
        this.editMode = true;
        this.tableView.setRowClick(this.editModeClick.bind(this));
      }
    },

    getFilterContent: function () {
      var filters = [];
      var i, value;

      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          value = $('#attribute_value' + i).val();

          try {
            value = JSON.parse(value);
          } catch (err) {
            value = String(value);
          }

          if ($('#attribute_name' + i).val() !== '') {
            filters.push({
              attribute: $('#attribute_name' + i).val(),
              operator: $('#operator' + i).val(),
              value: value
            });
          }
        }
      }
      return filters;
    },

    sendFilter: function () {
      this.restoredFilters = this.getFilterContent();
      var self = this;
      this.collection.resetFilter();
      this.addDocumentSwitch = false;
      _.each(this.restoredFilters, function (f) {
        if (f.operator !== undefined) {
          self.collection.addFilter(f.attribute, f.operator, f.value);
        }
      });
      this.collection.setToFirst();

      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.markFilterToggle();
    },

    restoreFilter: function () {
      var self = this;
      var counter = 0;

      this.filterId = 0;
      $('#docsSort').val(this.collection.getSort());
      _.each(this.restoredFilters, function (f) {
        // change html here and restore filters
        if (counter !== 0) {
          self.addFilterItem();
        }
        if (f.operator !== undefined) {
          $('#attribute_name' + counter).val(f.attribute);
          $('#operator' + counter).val(f.operator);
          $('#attribute_value' + counter).val(f.value);
        }
        counter++;

        // add those filters also to the collection
        self.collection.addFilter(f.attribute, f.operator, f.value);
      });

      self.rerender();
    },

    addFilterItem: function () {
      // adds a line to the filter widget

      var num = ++this.filterId;
      $('#filterHeader').append(' <div class="queryline querylineAdd">' +
        '<input id="attribute_name' + num +
        '" type="text" placeholder="Attribute name">' +
        '<select name="operator" id="operator' +
        num + '" class="filterSelect">' +
        '    <option value="==">==</option>' +
        '    <option value="!=">!=</option>' +
        '    <option value="&lt;">&lt;</option>' +
        '    <option value="&lt;=">&lt;=</option>' +
        '    <option value="&gt;">&gt;</option>' +
        '    <option value="&gt;=">&gt;=</option>' +
        '    <option value="LIKE">LIKE</option>' +
        '    <option value="IN">IN</option>' +
        '    <option value="=~">NOT IN</option>' +
        '    <option value="REGEX">REGEX</option>' +
        '</select>' +
        '<input id="attribute_value' + num +
        '" type="text" placeholder="Attribute value" ' +
        'class="filterValue">' +
        ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
        '<i class="fa fa-minus-circle"></i></a></div>');
      this.filters[num] = true;

      this.checkFilterState();
    },

    filterValueKeydown: function (e) {
      if (e.keyCode === 13) {
        this.sendFilter();
      }
    },

    checkFilterState: function () {
      var length = $('#filterHeader .queryline').length;

      if (length === 1) {
        $('#filterHeader .removeFilterItem').remove();
      } else {
        if ($('#filterHeader .queryline').first().find('.removeFilterItem').length === 0) {
          var id = $('#filterHeader .queryline').first().children().first().attr('id');
          var num = id.substr(14, id.length);

          $('#filterHeader .queryline').first().find('.add-filter-item').after(
            ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
            '<i class="fa fa-minus-circle"></i></a>');
        }
      }

      if ($('#filterHeader .queryline').first().find('.add-filter-item').length === 0) {
        $('#filterHeader .queryline').first().find('.filterValue').after(
          '<a id="addFilterItem" class="add-filter-item"><i style="margin-left: 4px;" class="fa fa-plus-circle"></i></a>'
        );
      }
    },

    removeFilterItem: function (e) {
      // removes line from the filter widget
      var button = e.currentTarget;

      var filterId = button.id.replace(/^removeFilter/, '');
      // remove the filter from the list
      delete this.filters[filterId];
      delete this.restoredFilters[filterId];

      // remove the line from the DOM
      $(button.parentElement).remove();
      this.checkFilterState();
    },

    removeAllFilterItems: function () {
      var childrenLength = $('#filterHeader').children().length;
      var i;
      for (i = 1; i <= childrenLength; i++) {
        $('#removeFilter' + i).parent().remove();
      }
      this.filters = { '0': true };
      this.filterId = 0;
    },

    addDocumentModal: function (e) {
      if (!$(e.currentTarget).hasClass('disabled')) {
        var collid = window.location.hash.split('/')[1];
        var buttons = []; var tableContent = [];
        // second parameter is "true" to disable caching of collection type

        var callback = function (error, type) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not fetch collection type');
          } else {
            if (type === 'edge') {
              tableContent.push(
                window.modalView.createTextEntry(
                  'new-edge-from-attr',
                  '_from',
                  '',
                  'document _id: document handle of the linked vertex (incoming relation)',
                  undefined,
                  false,
                  [
                    {
                      rule: Joi.string().required(),
                      msg: 'No _from attribute given.'
                    }
                  ]
                )
              );

              tableContent.push(
                window.modalView.createTextEntry(
                  'new-edge-to',
                  '_to',
                  '',
                  'document _id: document handle of the linked vertex (outgoing relation)',
                  undefined,
                  false,
                  [
                    {
                      rule: Joi.string().required(),
                      msg: 'No _to attribute given.'
                    }
                  ]
                )
              );

              tableContent.push(
                window.modalView.createTextEntry(
                  'new-edge-key-attr',
                  '_key',
                  undefined,
                  'the edges unique key(optional attribute, leave empty for autogenerated key',
                  'is optional: leave empty for autogenerated key',
                  false,
                  [
                    {
                      rule: Joi.string().allow('').optional(),
                      msg: ''
                    }
                  ]
                )
              );
              buttons.push(
                window.modalView.createSuccessButton('Create', this.addEdge.bind(this))
              );

              window.modalView.show(
                'modalTable.ejs',
                'Create edge',
                buttons,
                tableContent
              );
            } else {
              tableContent.push(
                window.modalView.createTextEntry(
                  'new-document-key-attr',
                  '_key',
                  undefined,
                  'the documents unique key(optional attribute, leave empty for autogenerated key',
                  'is optional: leave empty for autogenerated key',
                  false,
                  [
                    {
                      rule: Joi.string().allow('').optional(),
                      msg: ''
                    }
                  ]
                )
              );

              buttons.push(
                window.modalView.createSuccessButton('Create', this.addDocument.bind(this))
              );

              window.modalView.show(
                'modalTable.ejs',
                'Create document',
                buttons,
                tableContent
              );
            }
          }
        }.bind(this);
        arangoHelper.collectionApiType(collid, true, callback);
      }
    },

    addEdge: function () {
      var collid = window.location.hash.split('/')[1];
      var from = $('.modal-body #new-edge-from-attr').last().val();
      var to = $('.modal-body #new-edge-to').last().val();
      var key = $('.modal-body #new-edge-key-attr').last().val();
      var url;

      var callback = function (error, data, msg) {
        if (error) {
          arangoHelper.arangoError('Error', msg.errorMessage);
        } else {
          window.modalView.hide();
          data = data._id.split('/');

          try {
            url = 'collection/' + data[0] + '/' + data[1];
            decodeURI(url);
          } catch (ex) {
            url = 'collection/' + data[0] + '/' + encodeURIComponent(data[1]);
          }
          window.location.hash = url;
        }
      };

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeEdge(collid, from, to, key, callback);
      } else {
        this.documentStore.createTypeEdge(collid, from, to, null, callback);
      }
    },

    addDocument: function () {
      var collid = window.location.hash.split('/')[1];
      var key = $('.modal-body #new-document-key-attr').last().val();
      var url;

      var callback = function (error, data, msg) {
        if (error) {
          arangoHelper.arangoError('Error', msg.errorMessage);
        } else {
          window.modalView.hide();
          data = data.split('/');

          try {
            url = 'collection/' + data[0] + '/' + data[1];
            decodeURI(url);
          } catch (ex) {
            url = 'collection/' + data[0] + '/' + encodeURIComponent(data[1]);
          }

          window.location.hash = url;
        }
      };

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeDocument(collid, key, callback);
      } else {
        this.documentStore.createTypeDocument(collid, null, callback);
      }
    },

    moveSelectedDocs: function () {
      var buttons = []; var tableContent = [];
      var toDelete = this.getSelectedDocs();

      if (toDelete.length === 0) {
        return;
      }

      tableContent.push(
        window.modalView.createTextEntry(
          'move-documents-to',
          'Move to',
          '',
          false,
          'collection-name',
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: 'Collection name must always start with a letter.'
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: 'No collection name given.'
            }
          ]
        )
      );

      buttons.push(
        window.modalView.createSuccessButton('Move', this.confirmMoveSelectedDocs.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Move documents',
        buttons,
        tableContent
      );
    },

    confirmMoveSelectedDocs: function () {
      var toMove = this.getSelectedDocs();
      var self = this;
      var toCollection = $('#move-documents-to').val();

      var callback = function () {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
        $('#markDocuments').click();
        window.modalView.hide();
      }.bind(this);

      _.each(toMove, function (key) {
        self.collection.moveDocument(key, self.collection.collectionID, toCollection, callback);
      });
    },

    deleteSelectedDocs: function () {
      var buttons = []; var tableContent = [];
      var toDelete = this.getSelectedDocs();

      if (toDelete.length === 0) {
        return;
      }

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          toDelete.length + ' documents selected',
          'Do you want to delete all selected documents?',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.confirmDeleteSelectedDocs.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete documents',
        buttons,
        tableContent
      );
    },

    confirmDeleteSelectedDocs: function () {
      var toDelete = this.getSelectedDocs();
      var deleted = []; var self = this;

      _.each(toDelete, function (key) {
        if (self.type === 'document') {
          var callback = function (error) {
            if (error) {
              deleted.push(false);
              arangoHelper.arangoError('Document error', 'Could not delete document.');
            } else {
              deleted.push(true);
              self.collection.setTotalMinusOne();
              self.collection.getDocuments(this.getDocsCallback.bind(this));
              $('#markDocuments').click();
              window.modalView.hide();
            }
          }.bind(self);
          self.documentStore.deleteDocument(self.collection.collectionID, key, callback);
        } else if (self.type === 'edge') {
          var callback2 = function (error) {
            if (error) {
              deleted.push(false);
              arangoHelper.arangoError('Edge error', 'Could not delete edge');
            } else {
              self.collection.setTotalMinusOne();
              deleted.push(true);
              self.collection.getDocuments(this.getDocsCallback.bind(this));
              $('#markDocuments').click();
              window.modalView.hide();
            }
          }.bind(self);

          self.documentStore.deleteEdge(self.collection.collectionID, key, callback2);
        }
      });
    },

    getSelectedDocs: function () {
      var toDelete = [];
      _.each($('#docPureTable .pure-table-body .pure-table-row'), function (element) {
        if ($(element).hasClass('selected-row')) {
          toDelete.push($($(element).children()[1]).find('.key').text());
        }
      });
      return toDelete;
    },

    remove: function (a) {
      if (!$(a.currentTarget).hasClass('disabled')) {
        this.docid = $(a.currentTarget).parent().parent().prev().find('.key').text();
        $('#confirmDeleteBtn').attr('disabled', false);
        $('#docDeleteModal').modal('show');
      }
    },

    confirmDelete: function () {
      $('#confirmDeleteBtn').attr('disabled', true);
      var hash = window.location.hash.split('/');
      var check = hash[3];
      // find wrong event handler
      if (check !== 'source') {
        this.reallyDelete();
      }
    },

    reallyDelete: function () {
      if (this.type === 'document') {
        var callback = function (error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not delete document');
          } else {
            this.collection.setTotalMinusOne();
            this.collection.getDocuments(this.getDocsCallback.bind(this));
            $('#docDeleteModal').modal('hide');
          }
        }.bind(this);

        this.documentStore.deleteDocument(this.collection.collectionID, this.docid, callback);
      } else if (this.type === 'edge') {
        var callback2 = function (error) {
          if (error) {
            arangoHelper.arangoError('Edge error', 'Could not delete edge');
          } else {
            this.collection.setTotalMinusOne();
            this.collection.getDocuments(this.getDocsCallback.bind(this));
            $('#docDeleteModal').modal('hide');
          }
        }.bind(this);

        this.documentStore.deleteEdge(this.collection.collectionID, this.docid, callback2);
      }
    },

    editModeClick: function (event) {
      var target = $(event.currentTarget);

      if (target.hasClass('selected-row')) {
        target.removeClass('selected-row');
      } else {
        target.addClass('selected-row');
      }

      var selected = this.getSelectedDocs();
      $('.selectedCount').text(selected.length);

      _.each(this.editButtons, function (button) {
        if (selected.length > 0) {
          $(button).prop('disabled', false);
          $(button).removeClass('button-neutral');
          $(button).removeClass('disabled');
          if (button === '#moveSelected') {
            $(button).addClass('button-success');
          } else {
            $(button).addClass('button-danger');
          }
        } else {
          $(button).prop('disabled', true);
          $(button).addClass('disabled');
          $(button).addClass('button-neutral');
          if (button === '#moveSelected') {
            $(button).removeClass('button-success');
          } else {
            $(button).removeClass('button-danger');
          }
        }
      });
    },

    clicked: function (event) {
      var self = event.currentTarget;

      var url; var doc = $(self).attr('id').substr(4);

      try {
        url = 'collection/' + this.collection.collectionID + '/' + doc;
        decodeURI(doc);
      } catch (ex) {
        url = 'collection/' + this.collection.collectionID + '/' + encodeURIComponent(doc);
      }

      window.location.hash = url;
    },

    drawTable: function () {
      this.tableView.setElement($('#docPureTable')).render();
      // we added some icons, so we need to fix their tooltips
      arangoHelper.fixTooltips('.icon_arangodb, .arangoicon', 'top');

      $('.prettify').snippet('javascript', {
        style: 'nedit',
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });
      this.resize();
    },

    checkCollectionState: function () {
      if (this.lastCollectionName === this.collectionName) {
        if (this.activeFilter) {
          this.filterCollection();
          this.restoreFilter();
        }
      } else {
        if (this.lastCollectionName !== undefined) {
          this.collection.resetFilter();
          this.collection.setSort('');
          this.restoredFilters = [];
          this.activeFilter = false;
        }
      }
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      if (this.type === 2) {
        this.type = 'document';
      } else if (this.type === 3) {
        this.type = 'edge';
      }

      this.tableView.setElement($(this.table)).drawLoading();

      this.collectionContext = this.collectionsStore.getPosition(
        this.collection.collectionID
      );

      this.collectionName = window.location.hash.split('/')[1];

      this.checkCollectionState();

      // set last active collection name
      this.lastCollectionName = this.collectionName;

      /*
      if (this.collectionContext.prev === null) {
        $('#collectionPrev').parent().addClass('disabledPag')
      }
      if (this.collectionContext.next === null) {
        $('#collectionNext').parent().addClass('disabledPag')
      }
      */

      this.uploadSetup();

      arangoHelper.fixTooltips(['.icon_arangodb', '.arangoicon', 'top', '[data-toggle=tooltip]', '.upload-info']);
      this.renderPaginationElements();
      this.selectActivePagesize();
      this.markFilterToggle();
      this.resize();

      // fill navigation and breadcrumb
      this.breadcrumb();

      return this;
    },

    changeViewToReadOnly: function () {
      // this method disables all write-based functions
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
      $('#importCollection').addClass('disabled');
      $('#markDocuments').addClass('disabled');
      // create new document
      $('#addDocumentButton').addClass('disabled');
      $('#addDocumentButton .fa-plus-circle').css('color', '#d3d3d3');
      $('#addDocumentButton .fa-plus-circle').css('cursor', 'not-allowed');
      // delete a document
      $('.deleteButton').addClass('disabled');
      $('.deleteButton .fa-minus-circle').css('color', '#d3d3d3');
      $('.deleteButton .fa-minus-circle').css('cursor', 'not-allowed');
    },

    rerender: function () {
      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.resize();
    },

    selectActivePagesize: function () {
      $('#documentSize').val(this.collection.getPageSize());
    },

    renderPaginationElements: function () {
      this.renderPagination();
      var total = $('#totalDocuments');
      if (total.length === 0) {
        $('#documentsToolbarFL').append(
          '<a id="totalDocuments" class="totalDocuments"></a>'
        );
        total = $('#totalDocuments');
      }
      if (this.type === 'document') {
        total.html(numeral(this.collection.getTotal()).format('0,0') + ' doc(s)');
      }
      if (this.type === 'edge') {
        total.html(numeral(this.collection.getTotal()).format('0,0') + ' edge(s)');
      }
      if (this.collection.getTotal() > this.collection.MAX_SORT) {
        $('#docsSort').attr('disabled', true);
        $('#docsSort').attr('placeholder', 'Sort limit reached (docs count)');
      } else {
        $('#docsSort').attr('disabled', false);
        $('#docsSort').attr('placeholder', 'Sort by attribute');
      }
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'Collection: ' + this.collectionName
        );
        window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Content');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
