/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, _, $, window, arangoHelper, templateEngine, Joi, btoa */

(function() {
  "use strict";
  window.DocumentsView = window.PaginationView.extend({
    filters : { "0" : true },
    filterId : 0,
    paginationDiv : "#documentsToolbarF",
    idPrefix : "documents",

    addDocumentSwitch: true,
    activeFilter: false,
    lastCollectionName: undefined,
    restoredFilters: [],

    editMode: false,

    allowUpload: false,

    el: '#content',
    table: '#documentsTableID',

    template: templateEngine.createTemplate("documentsView.ejs"),

    collectionContext : {
      prev: null,
      next: null
    },

    editButtons: ["#deleteSelected", "#moveSelected"],

    initialize : function () {
      this.documentStore = this.options.documentStore;
      this.collectionsStore = this.options.collectionsStore;
      this.tableView = new window.TableView({
        el: this.table,
        collection: this.collection
      });
      this.tableView.setRowClick(this.clicked.bind(this));
      this.tableView.setRemoveClick(this.remove.bind(this));
    },

    setCollectionId : function (colid, page) {
      this.collection.setCollection(colid);
      this.collection.setPage(page);
      var type = arangoHelper.collectionApiType(colid);
      this.page = page;
      this.type = type;

      this.checkCollectionState();

      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.collectionModel = this.collectionsStore.get(colid);
    },

    getDocsCallback: function(error) {
      //Hide first/last pagination
      $('#documents_last').css("visibility", "hidden");
      $('#documents_first').css("visibility", "hidden");

      if (error) {
        window.progressView.hide();
        arangoHelper.arangoError("Document error", "Could not fetch requested documents.");
      }
      else if (!error || error !== undefined){
        window.progressView.hide();
        this.drawTable();
        this.renderPaginationElements();
      }

    },

    events: {
      "click #collectionPrev"      : "prevCollection",
      "click #collectionNext"      : "nextCollection",
      "click #filterCollection"    : "filterCollection",
      "click #markDocuments"       : "editDocuments",
      "click #importCollection"    : "importCollection",
      "click #exportCollection"    : "exportCollection",
      "click #filterSend"          : "sendFilter",
      "click #addFilterItem"       : "addFilterItem",
      "click .removeFilterItem"    : "removeFilterItem",
      "click #deleteSelected"      : "deleteSelectedDocs",
      "click #moveSelected"        : "moveSelectedDocs",
      "click #addDocumentButton"   : "addDocumentModal",
      "click #documents_first"     : "firstDocuments",
      "click #documents_last"      : "lastDocuments",
      "click #documents_prev"      : "prevDocuments",
      "click #documents_next"      : "nextDocuments",
      "click #confirmDeleteBtn"    : "confirmDelete",
      "click .key"                 : "nop",
      "keyup"                      : "returnPressedHandler",
      "keydown .queryline input"   : "filterValueKeydown",
      "click #importModal"         : "showImportModal",
      "click #resetView"           : "resetView",
      "click #confirmDocImport"    : "startUpload",
      "click #exportDocuments"     : "startDownload",
      "change #documentSize"            : "setPagesize",
      "change #docsSort"                : "setSorting"
    },

    showSpinner: function() {
      $('#uploadIndicator').show();
    },

    hideSpinner: function() {
      $('#uploadIndicator').hide();
    },

    showImportModal: function() {
      $("#docImportModal").modal('show');
    },

    hideImportModal: function() {
      $("#docImportModal").modal('hide');
    },

    setPagesize: function() {
      var size = $('#documentSize').find(":selected").val();
      this.collection.setPagesize(size);
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    setSorting: function() {
      var sortAttribute = $('#docsSort').val();

      if (sortAttribute === '' || sortAttribute === undefined || sortAttribute === null) {
        sortAttribute = '_key';
      }

      this.collection.setSort(sortAttribute);
    },

    returnPressedHandler: function(event) {
      if (event.keyCode === 13 && $(event.target).is($('#docsSort'))) {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
      }
      if (event.keyCode === 13) {
        if ($("#confirmDeleteBtn").attr("disabled") === false) {
          this.confirmDelete();
        }
      }
    },

    nop: function(event) {
      event.stopPropagation();
    },

    resetView: function () {
      //clear all input/select - fields
      $('input').val('');
      $('select').val('==');
      this.removeAllFilterItems();
      $('#documentSize').val(this.collection.getPageSize());

      $('#documents_last').css("visibility", "visible");
      $('#documents_first').css("visibility", "visible");
      this.addDocumentSwitch = true;
      this.collection.resetFilter();
      this.collection.loadTotal();
      this.restoredFilters = [];

      //for resetting json upload
      this.allowUpload = false;
      this.files = undefined;
      this.file = undefined;
      $('#confirmDocImport').attr("disabled", true);

      this.markFilterToggle();
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    startDownload: function() {
      var query = this.collection.buildDownloadDocumentQuery();

      if (query !== '' || query !== undefined || query !== null) {
        window.open(encodeURI("query/result/download/" + btoa(JSON.stringify(query))));
      }
      else {
        arangoHelper.arangoError("Document error", "could not download documents");
      }
    },

    startUpload: function () {
      var result;
      if (this.allowUpload === true) {
          this.showSpinner();
        result = this.collection.uploadDocuments(this.file);
        if (result !== true) {
          this.hideSpinner();
          this.hideImportModal();
          this.resetView();
          arangoHelper.arangoError(result);
          return;
        }
        this.hideSpinner();
        this.hideImportModal();
        this.resetView();
        return;
      }
    },

    uploadSetup: function () {
      var self = this;
      $('#importDocuments').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];
        $('#confirmDocImport').attr("disabled", false);

        self.allowUpload = true;
      });
    },

    buildCollectionLink : function (collection) {
      return "collection/" + encodeURIComponent(collection.get('name')) + '/documents/1';
    },
    /*
    prevCollection : function () {
      if (this.collectionContext.prev !== null) {
        $('#collectionPrev').parent().removeClass('disabledPag');
        window.App.navigate(
          this.buildCollectionLink(
            this.collectionContext.prev
          ),
          {
            trigger: true
          }
        );
      }
      else {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
    },

    nextCollection : function () {
      if (this.collectionContext.next !== null) {
        $('#collectionNext').parent().removeClass('disabledPag');
        window.App.navigate(
          this.buildCollectionLink(
            this.collectionContext.next
          ),
          {
            trigger: true
          }
        );
      }
      else {
        $('#collectionNext').parent().addClass('disabledPag');
      }
    },*/

    markFilterToggle: function () {
      if (this.restoredFilters.length > 0) {
        $('#filterCollection').addClass('activated');
      }
      else {
        $('#filterCollection').removeClass('activated');
      }
    },

    //need to make following functions automatically!

    editDocuments: function () {
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      this.markFilterToggle();
      $('#markDocuments').toggleClass('activated');
      this.changeEditMode();
      $('#filterHeader').hide();
      $('#importHeader').hide();
      $('#editHeader').slideToggle(200);
      $('#exportHeader').hide();
    },

    filterCollection : function () {
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      this.markFilterToggle();
      this.activeFilter = true;
      $('#importHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
      $('#filterHeader').slideToggle(200);

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
      $('#exportHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#filterHeader').hide();
      $('#editHeader').hide();
    },

    importCollection: function () {
      this.markFilterToggle();
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#importCollection').toggleClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#importHeader').slideToggle(200);
      $('#filterHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
    },

    changeEditMode: function (enable) {
      if (enable === false || this.editMode === true) {
        $('#documentsTableID tbody tr').css('cursor', 'default');
        $('.deleteButton').fadeIn();
        $('.addButton').fadeIn();
        $('.selected-row').removeClass('selected-row');
        this.editMode = false;
        this.tableView.setRowClick(this.clicked.bind(this));
      }
      else {
        $('#documentsTableID tbody tr').css('cursor', 'copy');
        $('.deleteButton').fadeOut();
        $('.addButton').fadeOut();
        $('.selectedCount').text(0);
        this.editMode = true;
        this.tableView.setRowClick(this.editModeClick.bind(this));
      }
    },

    getFilterContent: function () {
      var filters = [ ];
      var i, value;

      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          value = $('#attribute_value' + i).val();

          try {
            value = JSON.parse(value);
          }
          catch (err) {
            value = String(value);
          }

          if ($('#attribute_name' + i).val() !== ''){
            filters.push({
                attribute : $('#attribute_name'+i).val(),
                operator : $('#operator'+i).val(),
                value : value
            });
          }
        }
      }
      return filters;
    },

    sendFilter : function () {
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
      var self = this, counter = 0;

      this.filterId = 0;
      $('#docsSort').val(this.collection.getSort());
      _.each(this.restoredFilters, function (f) {
        //change html here and restore filters
        if (counter !== 0) {
          self.addFilterItem();
        }
        if (f.operator !== undefined) {
          $('#attribute_name' + counter).val(f.attribute);
          $('#operator' + counter).val(f.operator);
          $('#attribute_value' + counter).val(f.value);
        }
        counter++;

        //add those filters also to the collection
        self.collection.addFilter(f.attribute, f.operator, f.value);
      });
    },

    addFilterItem : function () {
      // adds a line to the filter widget

      var num = ++this.filterId;
      $('#filterHeader').append(' <div class="queryline querylineAdd">'+
                                '<input id="attribute_name' + num +
                                '" type="text" placeholder="Attribute name">'+
                                '<select name="operator" id="operator' +
                                num + '" class="filterSelect">'+
                                '    <option value="==">==</option>'+
                                '    <option value="!=">!=</option>'+
                                '    <option value="&lt;">&lt;</option>'+
                                '    <option value="&lt;=">&lt;=</option>'+
                                '    <option value="&gt;=">&gt;=</option>'+
                                '    <option value="&gt;">&gt;</option>'+
                                '</select>'+
                                '<input id="attribute_value' + num +
                                '" type="text" placeholder="Attribute value" ' +
                                'class="filterValue">'+
                                ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
                                '<i class="icon icon-minus arangoicon"></i></a></div>');
      this.filters[num] = true;
    },

    filterValueKeydown : function (e) {
      if (e.keyCode === 13) {
        this.sendFilter();
      }
    },

    removeFilterItem : function (e) {

      // removes line from the filter widget
      var button = e.currentTarget;

      var filterId = button.id.replace(/^removeFilter/, '');
      // remove the filter from the list
      delete this.filters[filterId];
      delete this.restoredFilters[filterId];

      // remove the line from the DOM
      $(button.parentElement).remove();
    },

    removeAllFilterItems : function () {
      var childrenLength = $('#filterHeader').children().length;
      var i;
      for (i = 1; i <= childrenLength; i++) {
        $('#removeFilter'+i).parent().remove();
      }
      this.filters = { "0" : true };
      this.filterId = 0;
    },

    addDocumentModal: function () {
      var collid  = window.location.hash.split("/")[1],
      buttons = [], tableContent = [],
      // second parameter is "true" to disable caching of collection type
      doctype = arangoHelper.collectionApiType(collid, true);
      if (doctype === 'edge') {

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-from-attr',
            '_from',
            '',
            "document _id: document handle of the linked vertex (incoming relation)",
            undefined,
            false,
            [
              {
                rule: Joi.string().required(),
                msg: "No _from attribute given."
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-to',
            '_to',
            '',
            "document _id: document handle of the linked vertex (outgoing relation)",
            undefined,
            false,
            [
              {
                rule: Joi.string().required(),
                msg: "No _to attribute given."
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-key-attr',
            '_key',
            undefined,
            "the edges unique key(optional attribute, leave empty for autogenerated key",
            'is optional: leave empty for autogenerated key',
            false,
            [
              {
                rule: Joi.string().allow('').optional(),
                msg: ""
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
      }
      else {
        tableContent.push(
          window.modalView.createTextEntry(
            'new-document-key-attr',
            '_key',
            undefined,
            "the documents unique key(optional attribute, leave empty for autogenerated key",
            'is optional: leave empty for autogenerated key',
            false,
            [
              {
                rule: Joi.string().allow('').optional(),
                msg: ""
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
    },

    addEdge: function () {
      var collid  = window.location.hash.split("/")[1];
      var from = $('.modal-body #new-edge-from-attr').last().val();
      var to = $('.modal-body #new-edge-to').last().val();
      var key = $('.modal-body #new-edge-key-attr').last().val();

      var result;
      if (key !== '' || key !== undefined) {
        result = this.documentStore.createTypeEdge(collid, from, to, key);
      }
      else {
        result = this.documentStore.createTypeEdge(collid, from, to);
      }

      if (result !== false) {
        //$('#edgeCreateModal').modal('hide');
        window.modalView.hide();
        window.location.hash = "collection/"+result;
      }
      //Error
      else {
        arangoHelper.arangoError('Edge error', 'Creation failed.');
      }
    },

    addDocument: function() {
      var collid = window.location.hash.split("/")[1];
      var key = $('.modal-body #new-document-key-attr').last().val();
      var result;
      if (key !== '' || key !== undefined) {
        result = this.documentStore.createTypeDocument(collid, key);
      }
      else {
        result = this.documentStore.createTypeDocument(collid);
      }
      //Success
      if (result !== false) {
        window.modalView.hide();
        window.location.hash = "collection/" + result;
      }
      else {
        arangoHelper.arangoError('Document error', 'Creation failed.');
      }
    },

    moveSelectedDocs: function() {
      var buttons = [], tableContent = [],
      toDelete = this.getSelectedDocs();

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
              msg: "Collection name must always start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No collection name given."
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

    confirmMoveSelectedDocs: function() {
      var toMove = this.getSelectedDocs(),
      self = this,
      toCollection = $('.modal-body').last().find('#move-documents-to').val();

      var callback = function() {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
        $('#markDocuments').click();
        window.modalView.hide();
      }.bind(this);

      _.each(toMove, function(key) {
        self.collection.moveDocument(key, self.collection.collectionID, toCollection, callback);
      });
    },

    deleteSelectedDocs: function() {
      var buttons = [], tableContent = [];
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

    confirmDeleteSelectedDocs: function() {
      var toDelete = this.getSelectedDocs();
      var deleted = [], self = this;

      _.each(toDelete, function(key) {
        var result = false;
        if (self.type === 'document') {
          result = self.documentStore.deleteDocument(
            self.collection.collectionID, key
          );
          if (result) {
            //on success
            deleted.push(true);
            self.collection.setTotalMinusOne();
          }
          else {
            deleted.push(false);
            arangoHelper.arangoError('Document error', 'Could not delete document.');
          }
        }
        else if (self.type === 'edge') {
          result = self.documentStore.deleteEdge(self.collection.collectionID, key);
          if (result === true) {
            //on success
            self.collection.setTotalMinusOne();
            deleted.push(true);
          }
          else {
            deleted.push(false);
            arangoHelper.arangoError('Edge error', 'Could not delete edge');
          }
        }
      });
      this.collection.getDocuments(this.getDocsCallback.bind(this));
      $('#markDocuments').click();
      window.modalView.hide();
    },

    getSelectedDocs: function() {
      var toDelete = [];
      _.each($('#documentsTableID tbody tr'), function(element) {
        if ($(element).hasClass('selected-row')) {
          toDelete.push($($(element).children()[1]).find('.key').text());
        }
      });
      return toDelete;
    },

    remove: function (a) {
      this.docid = $(a.currentTarget).closest("tr").attr("id").substr(4);
      $("#confirmDeleteBtn").attr("disabled", false);
      $('#docDeleteModal').modal('show');
    },

    confirmDelete: function () {
      $("#confirmDeleteBtn").attr("disabled", true);
      var hash = window.location.hash.split("/");
      var check = hash[3];
      //to_do - find wrong event handler
      if (check !== 'source') {
        this.reallyDelete();
      }
    },

    reallyDelete: function () {
      var deleted = false;
      var result;
      if (this.type === 'document') {
        result = this.documentStore.deleteDocument(
          this.collection.collectionID, this.docid
        );
        if (result) {
          //on success
          this.collection.setTotalMinusOne();
          deleted = true;
        }
        else {
          arangoHelper.arangoError('Doc error');
        }
      }
      else if (this.type === 'edge') {
        result = this.documentStore.deleteEdge(this.collection.collectionID, this.docid);
        if (result === true) {
          //on success
          this.collection.setTotalMinusOne();
          deleted = true;
        }
        else {
          arangoHelper.arangoError('Edge error');
        }
      }

      if (deleted === true) {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
        $('#docDeleteModal').modal('hide');
      }
    },

    editModeClick: function(event) {
      var target = $(event.currentTarget);

      if(target.hasClass('selected-row')) {
        target.removeClass('selected-row');
      } else {
        target.addClass('selected-row');
      }

      var selected = this.getSelectedDocs();
      $('.selectedCount').text(selected.length);

      _.each(this.editButtons, function(button) {
        if (selected.length > 0) {
          $(button).prop('disabled', false);
          $(button).removeClass('button-neutral');
          $(button).removeClass('disabled');
          if (button === "#moveSelected") {
            $(button).addClass('button-success');
          }
          else {
            $(button).addClass('button-danger');
          }
        }
        else {
          $(button).prop('disabled', true);
          $(button).addClass('disabled');
          $(button).addClass('button-neutral');
          if (button === "#moveSelected") {
            $(button).removeClass('button-success');
          }
          else {
            $(button).removeClass('button-danger');
          }
        }
      });
    },

    clicked: function (event) {
      var self = event.currentTarget;
      window.App.navigate("collection/" + this.collection.collectionID + "/" + $(self).attr("id").substr(4), true);
    },

    drawTable: function() {
      this.tableView.setElement($(this.table)).render();

      // we added some icons, so we need to fix their tooltips
      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

      $(".prettify").snippet("javascript", {
        style: "nedit",
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });
    },

    checkCollectionState: function() {
      if (this.lastCollectionName === this.collectionName) {
        if (this.activeFilter) {
          this.filterCollection();
          console.log("yes");
          this.restoreFilter();
        }
      }
      else {
        if (this.lastCollectionName !== undefined) {
          this.collection.resetFilter();
          this.collection.setSort('_key');
          this.restoredFilters = [];
          this.activeFilter = false;
        }
      }
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.tableView.setElement($(this.table)).drawLoading();

      this.collectionContext = this.collectionsStore.getPosition(
        this.collection.collectionID
      );

      this.breadcrumb();

      this.checkCollectionState();

      //set last active collection name
      this.lastCollectionName = this.collectionName;

      /*
      if (this.collectionContext.prev === null) {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
      if (this.collectionContext.next === null) {
        $('#collectionNext').parent().addClass('disabledPag');
      }
      */

      this.uploadSetup();

      $("[data-toggle=tooltip]").tooltip();
      $('.upload-info').tooltip();

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");
      this.renderPaginationElements();
      this.selectActivePagesize();
      this.markFilterToggle();
      return this;
    },

    rerender: function () {
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    selectActivePagesize: function() {
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
        total.html(this.collection.getTotal() + " document(s)");
      }
      if (this.type === 'edge') {
        total.html(this.collection.getTotal() + " edge(s)");
      }
    },

    breadcrumb: function () {
      this.collectionName = window.location.hash.split("/")[1];
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a class="activeBread" href="#collections">Collections</a>'+
        '<span class="disabledBread"><i class="fa fa-chevron-right"></i></span>'+
        '<a class="disabledBread">'+this.collectionName+'</a>'+
        '</div>'
      );
    }

  });
}());
