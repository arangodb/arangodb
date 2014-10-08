/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, arangoHelper, _, $, window, arangoHelper, templateEngine */

(function() {
  "use strict";
  window.DocumentsView = window.PaginationView.extend({
    filters : { "0" : true },
    filterId : 0,
    paginationDiv : "#documentsToolbarF",
    idPrefix : "documents",

    addDocumentSwitch: true,

    allowUpload: false,

    collectionContext : {
      prev: null,
      next: null
    },

    initialize : function () {
      this.documentStore = this.options.documentStore;
      this.collectionsStore = this.options.collectionsStore;
    },


    setCollectionId : function (colid, pageid) {
      this.collection.setCollection(colid);
      var type = arangoHelper.collectionApiType(colid);
      this.pageid = pageid;
      this.type = type;
      this.collection.getDocuments(colid, pageid);
      this.collectionModel = this.collectionsStore.get(colid);
    },

    alreadyClicked: false,

    el: '#content',
    table: '#documentsTableID',


    template: templateEngine.createTemplate("documentsView.ejs"),

    events: {
      "click #collectionPrev"      : "prevCollection",
      "click #collectionNext"      : "nextCollection",
      "click #filterCollection"    : "filterCollection",
      "click #indexCollection"     : "indexCollection",
      "click #importCollection"    : "importCollection",
      "click #filterSend"          : "sendFilter",
      "click #addFilterItem"       : "addFilterItem",
      "click .removeFilterItem"    : "removeFilterItem",
      "click #confirmCreateEdge"   : "addEdge",
      "click #documentsTableID tr" : "clicked",
      "click #deleteDoc"           : "remove",
      "click #addDocumentButton"   : "addDocument",
      "click #documents_first"     : "firstDocuments",
      "click #documents_last"      : "lastDocuments",
      "click #documents_prev"      : "prevDocuments",
      "click #documents_next"      : "nextDocuments",
      "click #confirmDeleteBtn"    : "confirmDelete",
      "keyup #createEdge"          : "listenKey",
      "click .key"                 : "nop",
      "keyup"                      : "returnPressedHandler",
      "keydown .filterValue"       : "filterValueKeydown",
      "click #importModal"         : "showImportModal",
      "click #resetView"           : "resetView",
      "click #confirmDocImport"    : "startUpload",
      "change #newIndexType"       : "selectIndexType",
      "click #createIndex"         : "createIndex",
      "click .deleteIndex"         : "prepDeleteIndex",
      "click #confirmDeleteIndexBtn"    : "deleteIndex",
      "click #documentsToolbar ul"      : "resetIndexForms",
      "click #indexHeader #addIndex"    :    "toggleNewIndexView",
      "click #indexHeader #cancelIndex" :    "toggleNewIndexView"
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

    returnPressedHandler: function(event) {
      if (event.keyCode === 13) {
        if ($("#confirmDeleteBtn").attr("disabled") === false) {
          this.confirmDelete();
        }
      }
    },
    toggleNewIndexView: function () {
      $('#indexEditView').toggle("fast");
      $('#newIndexView').toggle("fast");
      this.resetIndexForms();
    },
    nop: function(event) {
      event.stopPropagation();
    },

    listenKey: function (e) {
      if(e.keyCode === 13){
        this.addEdge();
      }
    },

    resetView: function () {
      //clear all input/select - fields
      $('input').val('');
      $('select').val('==');
      this.removeAllFilterItems();

      this.clearTable();
      $('#documents_last').css("visibility", "visible");
      $('#documents_first').css("visibility", "visible");
      this.addDocumentSwitch = true;
      this.collection.resetFilter();
      this.collection.loadTotal();
      this.collection.getDocuments();
      this.drawTable();
      this.renderPaginationElements();
    },

    startUpload: function () {
      var result;
      if (this.allowUpload === true) {
          this.showSpinner();
        result = this.collection.updloadDocuments(this.file);
        if (result !== true) {
            this.hideSpinner();
            if (result.substr(0, 5 ) === "Error") {
                this.hideImportModal();
                this.resetView();
            }
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

        self.allowUpload = true;
      });
    },

    buildCollectionLink : function (collection) {
      return "collection/" + encodeURIComponent(collection.get('name')) + '/documents/1';
    },

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
    },

    filterCollection : function () {
      $('#indexCollection').removeClass('activated');
      $('#importCollection').removeClass('activated');
      $('#filterCollection').toggleClass('activated');
      $('#filterHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#indexHeader').hide();

      var i;
      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          $('#attribute_name' + i).focus();
          return;
        }
      }
    },

    importCollection: function () {
      $('#filterCollection').removeClass('activated');
      $('#indexCollection').removeClass('activated');
      $('#importCollection').toggleClass('activated');
      $('#importHeader').slideToggle(200);
      $('#filterHeader').hide();
      $('#indexHeader').hide();
    },

    indexCollection: function () {
      $('#filterCollection').removeClass('activated');
      $('#importCollection').removeClass('activated');
      $('#indexCollection').toggleClass('activated');
      $('#newIndexView').hide();
      $('#indexEditView').show();
      $('#indexHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#filterHeader').hide();

    },

    getFilterContent: function () {
      var filters = [ ];
      var i;

      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          var value = $('#attribute_value' + i).val();

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
      var filters = this.getFilterContent();
      var self = this;
      this.collection.resetFilter();
      this.addDocumentSwitch = false;
      _.each(filters, function (f) {
          self.collection.addFilter(f.attribute, f.operator, f.value);
      });
      this.clearTable();
      this.collection.setToFirst();
      this.collection.getDocuments();

      //Hide first/last pagination
      $('#documents_last').css("visibility", "hidden");
      $('#documents_first').css("visibility", "hidden");

      this.drawTable();
      this.renderPaginationElements();
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

    addDocument: function () {
      var collid  = window.location.hash.split("/")[1],
        // second parameter is "true" to disable caching of collection type
        doctype = arangoHelper.collectionApiType(collid, true);
      if (doctype === 'edge') {
        $('#edgeCreateModal').modal('show');
        arangoHelper.fixTooltips(".modalTooltips", "left");
        return;
      }

      var result = this.documentStore.createTypeDocument(collid);
      //Success
      if (result !== false) {
        window.location.hash = "collection/" + result;
        return;
      }
      //Error
      arangoHelper.arangoError('Creating document failed');
    },

    addEdge: function () {
      var collid  = window.location.hash.split("/")[1];
      var from = $('#new-document-from').val();
      var to = $('#new-document-to').val();
      if (from === '') {
        //Heiko: Form-Validator - from is missing
        return;
      }
      if (to === '') {
        //Heiko: Form-Validator - to is missing
        return;
      }
      var result = this.documentStore.createTypeEdge(collid, from, to);

      if (result !== false) {
        $('#edgeCreateModal').modal('hide');
        window.location.hash = "collection/"+result;
      }
      //Error
      else {
        arangoHelper.arangoError('Creating edge failed');
      }
    },

    remove: function (a) {
      this.target = a.currentTarget;
      var thiselement = a.currentTarget.parentElement;
      this.idelement = $(thiselement).prev().prev();
      this.alreadyClicked = true;
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
      var self = this;
      var row = $(self.target).closest("tr").get(0);
      var hash = window.location.hash.split("/");
      var page = hash[3];

      var deleted = false;
      this.docid = $(self.idelement).next().text();

      var result;
      if (this.type === 'document') {
        result = this.documentStore.deleteDocument(
          this.collection.collectionID, this.docid
        );
        if (result) {
          //on success
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
          deleted = true;
        }
        else {
          arangoHelper.arangoError('Edge error');
        }
      }

      if (deleted === true) {
        $('#documentsTableID').dataTable().fnDeleteRow(
          $('#documentsTableID').dataTable().fnGetPosition(row)
        );
        $('#documentsTableID').dataTable().fnClearTable();
          this.collection.getDocuments(this.collection.collectionID, page);
        $('#docDeleteModal').modal('hide');
        this.collection.loadTotal();
        this.drawTable();
        this.renderPaginationElements();
      }

    },
    clicked: function (event) {
      if (this.alreadyClicked === true) {
        this.alreadyClicked = false;
        return 0;
      }
      var self = event.currentTarget;
      var aPos = $(this.table).dataTable().fnGetPosition(self);
      if (aPos === null) {
        // headline
        return;
      }

      var checkData = $(this.table).dataTable().fnGetData(self);
      if (checkData && checkData[1] === '') {
        this.addDocument();
        return;
      }
      var docId = self.firstChild;
      var NeXt = $(docId).next().text();
      window.location.hash = "#collection/" + this.collection.collectionID
          + "/" + NeXt;
    },

    initTable: function () {
      $('#documentsTableID').dataTable({
        "bSortClasses": false,
        "bFilter": false,
        "bPaginate":false,
        "bRetrieve": true,
        "bSortable": false,
        "bSort": false,
        "bLengthChange": false,
        "bAutoWidth": false,
        "iDisplayLength": -1,
        "bJQueryUI": false,
        "aoColumns": [
          { "sClass":"docsFirstCol","bSortable": false},
          { "sClass":"docsSecCol", "bSortable": false},
          { "bSortable": false, "sClass": "docsThirdCol"}
        ],
        "oLanguage": { "sEmptyTable": "Loading..."}
      });
    },
    clearTable: function() {
      $(this.table).dataTable().fnClearTable();
    },
    drawTable: function() {
      this.clearTable();
      var self = this;

        if (this.collection.size() === 0) {
        $('.dataTables_empty').text('No documents');
      }
      else {
            this.collection.each(function(value, key) {
          var tempObj = {};
          $.each(value.attributes.content, function(k, v) {
            if (! (k === '_id' || k === '_rev' || k === '_key')) {
              tempObj[k] = v;
            }
          });

          $(self.table).dataTable().fnAddData(
            [
              '<pre class="prettify" title="'
              + self.escaped(JSON.stringify(tempObj))
              + '">'
              + self.cutByResolution(JSON.stringify(tempObj))
              + '</pre>',

              '<div class="key">'
              + value.attributes.key
              + '</div>',

              '<a id="deleteDoc" class="deleteButton">'
              + '<span class="icon_arangodb_roundminus" data-original-title="'
              +'Delete document" title="Delete document"></span><a>'
            ]
          );
        });

        // we added some icons, so we need to fix their tooltips
        arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

        $(".prettify").snippet("javascript", {
          style: "nedit",
          menu: false,
          startText: false,
          transparent: true,
          showNum: false
        });

      }
    },


    render: function() {
      this.collectionContext = this.collectionsStore.getPosition(
          this.collection.collectionID
      );

      $(this.el).html(this.template.render({}));
      this.getIndex();
      this.initTable();
      this.breadcrumb();
      if (this.collectionContext.prev === null) {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
      if (this.collectionContext.next === null) {
        $('#collectionNext').parent().addClass('disabledPag');
      }

      this.uploadSetup();

      $("[data-toggle=tooltip]").tooltip();

      $('.modalImportTooltips').tooltip({
        placement: "left"
      });

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");
      this.drawTable();
      this.renderPaginationElements();
      return this;
    },

    rerender : function () {
     // this.collection.loadTotal();
      this.clearTable();
      this.collection.getDocuments();
      this.drawTable();
      $('#documents_last').css("visibility", "hidden");
      $('#documents_first').css("visibility", "hidden");
      this.renderPaginationElements();
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
      total.html("Total: " + this.collection.getTotal() + " document(s)");
    },

    breadcrumb: function () {
      this.collectionName = window.location.hash.split("/")[1];
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a class="activeBread" href="#collections">Collections</a>'+
        '  &gt;  '+
        '<a class="disabledBread">'+this.collectionName+'</a>'+
        '</div>'
      );
    },
    cutByResolution: function (string) {
      if (string.length > 1024) {
        return this.escaped(string.substr(0, 1024)) + '...';
      }
      return this.escaped(string);
    },
    escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    },
    resetIndexForms: function () {
      $('#indexHeader input').val('').prop("checked", false);
      $('#newIndexType').val('Cap').prop('selected',true);
      this.selectIndexType();
    },
    stringToArray: function (fieldString) {
      var fields = [];
      fieldString.split(',').forEach(function(field){
        field = field.replace(/(^\s+|\s+$)/g,'');
        if (field !== '') {
          fields.push(field);
        }
      });
      return fields;
    },
    createIndex: function () {
      //e.preventDefault();
      var self = this;
      var indexType = $('#newIndexType').val();
      var result;
      var postParameter = {};
      var fields;
      var unique;

      switch(indexType) {
        case 'Cap':
          var size = parseInt($('#newCapSize').val(), 10) || 0;
          var byteSize = parseInt($('#newCapByteSize').val(), 10) || 0;
          postParameter = {
            type: 'cap',
            size: size,
            byteSize: byteSize
          };
          break;
        case 'Geo':
          //HANDLE ARRAY building
          fields = $('#newGeoFields').val();
          var geoJson = self.checkboxToValue('#newGeoJson');
          var constraint = self.checkboxToValue('#newGeoConstraint');
          var ignoreNull = self.checkboxToValue('#newGeoIgnoreNull');
          postParameter = {
            type: 'geo',
            fields: self.stringToArray(fields),
            geoJson: geoJson,
            constraint: constraint,
            ignoreNull: ignoreNull
          };
          break;
        case 'Hash':
          fields = $('#newHashFields').val();
          unique = self.checkboxToValue('#newHashUnique');
          postParameter = {
            type: 'hash',
            fields: self.stringToArray(fields),
            unique: unique
          };
          break;
        case 'Fulltext':
          fields = ($('#newFulltextFields').val());
          var minLength =  parseInt($('#newFulltextMinLength').val(), 10) || 0;
          postParameter = {
            type: 'fulltext',
            fields: self.stringToArray(fields),
            minLength: minLength
          };
          break;
        case 'Skiplist':
          fields = $('#newSkiplistFields').val();
          unique = self.checkboxToValue('#newSkiplistUnique');
          postParameter = {
            type: 'skiplist',
            fields: self.stringToArray(fields),
            unique: unique
          };
          break;
      }
      result = self.collectionModel.createIndex(postParameter);
      if (result === true) {
        $('#collectionEditIndexTable tr').remove();
        self.getIndex();
        self.toggleNewIndexView();
        self.resetIndexForms();
      }
      else {
        if (result.responseText) {
          var message = JSON.parse(result.responseText);
          arangoHelper.arangoNotification("Document error", message.errorMessage);
        }
        else {
          arangoHelper.arangoNotification("Document error", "Could not create index.");
        }
      }
    },

    prepDeleteIndex: function (e) {
      this.lastTarget = e;
      this.lastId = $(this.lastTarget.currentTarget).
                    parent().
                    parent().
                    first().
                    children().
                    first().
                    text();
      $("#indexDeleteModal").modal('show');
    },
    deleteIndex: function () {
      var result = this.collectionModel.deleteIndex(this.lastId);
      if (result === true) {
        $(this.lastTarget.currentTarget).parent().parent().remove();
      }
      else {
        arangoHelper.arangoError("Could not delete index");
      }
      $("#indexDeleteModal").modal('hide');
    },
    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType'+type).show();
    },
    checkboxToValue: function (id) {
      return $(id).prop('checked');
    },
    getIndex: function () {
      this.index = this.collectionModel.getIndex();
      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        $.each(this.index.indexes, function(k,v) {
          if (v.type === 'primary' || v.type === 'edge') {
            actionString = '<span class="icon_arangodb_locked" ' +
              'data-original-title="No action"></span>';
          }
          else {
            actionString = '<span class="deleteIndex icon_arangodb_roundminus" ' +
              'data-original-title="Delete index" title="Delete index"></span>';
          }

          if (v.fields !== undefined) {
            fieldString = v.fields.join(", ");
          }

          //cut index id
          var position = v.id.indexOf('/');
          var indexId = v.id.substr(position+1, v.id.length);

          $('#collectionEditIndexTable').append(
            '<tr>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + actionString + '</th>'+
            '</tr>'
          );
        });

        arangoHelper.fixTooltips("deleteIndex", "left");
      }
    }
  });
}());
