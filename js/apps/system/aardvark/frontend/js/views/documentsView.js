/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper */

var documentsView = Backbone.View.extend({
  collectionID: 0,
  currentPage: 1,
  documentsPerPage: 10,
  totalPages: 1,
  filters : { "0" : true },
  filterId : 0,

  addDocumentSwitch: true,

  allowUpload: false,

  collectionContext : {
    prev: null,
    next: null
  },

  alreadyClicked: false,

  el: '#content',
  table: '#documentsTableID',

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
    window.arangoDocumentsStore.getDocuments(this.collectionID, 1);
  },

  startUpload: function () {
    var self = this;
    if (self.allowUpload === true) {
      self.showSpinner();
      $.ajax({
        type: "POST",
        async: false,
        url:
          '/_api/import?type=auto&collection='+
          encodeURIComponent(self.colid)+
          '&createCollection=false',
        data: self.file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function(xhr) {
          if (xhr.readyState === 4) {
            if (xhr.status === 201) {
              var data;

              try {
                data = JSON.parse(xhr.responseText);
              }

              catch (e) {
                arangoHelper.arangoNotification("Error: "+ e);
                self.hideSpinner();
                self.hideImportModal();
                self.resetView();
                return;
              }

              if (data.errors === 0) {
                arangoHelper.arangoError("Upload successful. " + 
                                         data.created + " document(s) imported.");
              }
              else if (data.errors !== 0) {
                arangoHelper.arangoError("Upload failed." + 
                                         data.errors + "error(s).");
              }
              self.hideSpinner();
              self.hideImportModal();
              self.resetView();
              return;
            }
          }
          self.hideSpinner();
          arangoHelper.arangoNotification("Upload error");
        }
      });
    }
    else {
      arangoHelper.arangoNotification("Unsupported filetype: " + self.file.type);
    }
  },

  uploadSetup: function () {
    var self = this;

    //$('#documentsUploadFile').change(function(e) {
    $('#importDocuments').change(function(e) {
      self.files = e.target.files || e.dataTransfer.files;
      self.file = self.files[0];

      if (self.file.type !== 'application/json') {
        arangoHelper.arangoNotification("Unsupported filetype: " + self.file.type);
        self.allowUpload = false;
      }
      else {
        self.allowUpload = true;
      }
    });
  },

  buildCollectionLink : function (collection) {
    return "collection/" + encodeURIComponent(collection.get('name')) + '/documents/1';
  },

  prevCollection : function () {
    if (this.collectionContext.prev !== null) {
      $('#collectionPrev').parent().removeClass('disabledPag');
      window.App.navigate(this.buildCollectionLink(this.collectionContext.prev), { trigger: true });
    }
    else {
      $('#collectionPrev').parent().addClass('disabledPag');
    }
  },

  nextCollection : function () {
    if (this.collectionContext.next !== null) {
      $('#collectionNext').parent().removeClass('disabledPag');
      window.App.navigate(this.buildCollectionLink(this.collectionContext.next), { trigger: true });
    }
    else {
      $('#collectionNext').parent().addClass('disabledPag');
    }
  },

  filterCollection : function () {
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
    $('#importHeader').slideToggle(200);
    $('#filterHeader').hide();
    $('#indexHeader').hide();
  },

  indexCollection: function () {
    $('#newIndexView').hide();
    $('#indexEditView').show();
    $('#indexHeader').slideToggle(200);
    $('#importHeader').hide();
    $('#filterHeader').hide();

  },

  getFilterContent: function () {
    var filters = [ ], bindValues = { };
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
          filters.push(" u.`"+ $('#attribute_name'+i).val() + "`" + 
                       $('#operator' + i).val() + 
                       "@param" + i);
          bindValues["param" + i] = value;
        }
      }
    }
    return [filters, bindValues];
  },

  sendFilter : function () {
    var filterArray = this.getFilterContent();
    var filters = filterArray[0];
    var bindValues = filterArray[1];
    this.addDocumentSwitch = false;
    window.documentsView.clearTable();
    window.arangoDocumentsStore.getFilteredDocuments(this.colid, 1, filters, bindValues);

    //Hide first/last pagination
    $('#documents_last').css("visibility", "hidden");
    $('#documents_first').css("visibility", "hidden");
  },

  addFilterItem : function () {
    "use strict";
    // adds a line to the filter widget

    var num = ++this.filterId;
    $('#filterHeader').append(' <div class="queryline querylineAdd">'+
                              '<input id="attribute_name' + num + 
                              '" type="text" placeholder="Attribute name">'+
                              '<select name="operator" id="operator' + num + '">'+
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
                              '<i class="icon icon-minus icon-white"></i></a>'+
                              ' <span>AND</span></div>');
    this.filters[num] = true;
  },

  filterValueKeydown : function (e) {
    if (e.keyCode === 13) {
      this.sendFilter();
    }
  },

  removeFilterItem : function (e) {
    "use strict";

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
    var collid  = window.location.hash.split("/")[1];
    var doctype = arangoHelper.collectionApiType(collid);

    if (doctype === 'edge') {
      $('#edgeCreateModal').modal('show');
      arangoHelper.fixTooltips(".modalTooltips", "left");
    }
    else {
      var result = window.arangoDocumentStore.createTypeDocument(collid);
      //Success
      if (result !== false) {
        window.location.hash = "collection/" + result;
        arangoHelper.arangoNotification('Document created');
      }
      //Error
      else {
        arangoHelper.arangoError('Creating document failed');
      }
    }
  },
  addEdge: function () {
    var collid  = window.location.hash.split("/")[1];
    var from = $('#new-document-from').val();
    var to = $('#new-document-to').val();
    if (from === '') {
      arangoHelper.arangoNotification('From paramater is missing');
      return;
    }
    if (to === '') {
      arangoHelper.arangoNotification('To parameter is missing');
      return;
    }
    var result = window.arangoDocumentStore.createTypeEdge(collid, from, to);

    if (result !== false) {
      $('#edgeCreateModal').modal('hide');
      window.location.hash = "collection/"+result;
    }
    //Error
    else {
      arangoHelper.arangoError('Creating edge failed');
    }
  },
  firstDocuments: function () {
    window.arangoDocumentsStore.getFirstDocuments();
  },
  lastDocuments: function () {
    window.arangoDocumentsStore.getLastDocuments();
  },
  prevDocuments: function () {
    window.arangoDocumentsStore.getPrevDocuments();
  },
  nextDocuments: function () {
    window.arangoDocumentsStore.getNextDocuments();
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
    //todo - find wrong event handler
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
      result = window.arangoDocumentStore.deleteDocument(this.colid, this.docid);
      if (result === true) {
        //on success
        arangoHelper.arangoNotification('Document deleted');
        deleted = true;
      }
      else if (result === false) {
        arangoHelper.arangoError('Could not delete document');
      }
    }
    else if (this.type === 'edge') {
      result = window.arangoDocumentStore.deleteEdge(this.colid, this.docid);
      if (result === true) {
        //on success
        arangoHelper.arangoNotification('Edge deleted');
        deleted = true;
      }
      else if (result === false) {
        arangoHelper.arangoError('Edge error');
      }
    }

    if (deleted === true) {
      $('#documentsTableID').dataTable().fnDeleteRow(
        $('#documentsTableID').dataTable().fnGetPosition(row)
      );
      $('#documentsTableID').dataTable().fnClearTable();
      window.arangoDocumentsStore.getDocuments(this.colid, page);
      $('#docDeleteModal').modal('hide');
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
    window.location.hash = "#collection/" + this.colid + "/" + NeXt;
  },

  initTable: function (colid, pageid) {
    var documentsTable = $('#documentsTableID').dataTable({
      "aaSorting": [[ 1, "asc" ]],
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
    var self = this;

    /*
       if (this.addDocumentSwitch === true) {
       $(self.table).dataTable().fnAddData(
       [
       '<a id="plusIconDoc" style="padding-left: 30px">Add document</a>',
       '',
       '<img src="img/plus_icon.png" id="documentAddBtn"></img>'
       ]
       );
       }*/

    if (window.arangoDocumentsStore.models.length === 0) {
      $('.dataTables_empty').text('No documents');
    }
    else {

      $.each(window.arangoDocumentsStore.models, function(key, value) {

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

            '<a id="deleteDoc"><span class="glyphicon glyphicon-minus-sign" data-original-title="'
            +'Delete document" title="Delete document"></span><a>'
          ]
        );
      });

      // we added some icons, so we need to fix their tooltips
      arangoHelper.fixTooltips(".glyphicon, .arangoicon", "top");

      $(".prettify").snippet("javascript", {
        style: "nedit",
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });

    }
    this.totalPages = window.arangoDocumentsStore.totalPages;
    this.currentPage = window.arangoDocumentsStore.currentPage;
    this.documentsCount = window.arangoDocumentsStore.documentsCount;
    if (this.documentsCount !== 0) {
      $('#documentsStatus').html(
        'Showing Page '+this.currentPage+' of '+this.totalPages+
        ', '+this.documentsCount+' entries'
      );
    }
  },

  template: new EJS({url: 'js/templates/documentsView.ejs'}),

  render: function() {
    this.collectionContext = window.arangoCollectionsStore.getPosition(this.colid);

    $(this.el).html(this.template.text);
    this.getIndex();
    this.initTable();
    this.breadcrumb();
    if (this.collectionContext.prev === null) {
      $('#collectionPrev').parent().addClass('disabledPag');
    }
    if (this.collectionContext.next === null) {
      $('#collectionNext').parent().addClass('disabledPag');
    }
    $.gritter.removeAll();

    this.uploadSetup();

    $('.modalImportTooltips').tooltip({
      placement: "left"
    });
      
    arangoHelper.fixTooltips(".glyphicon, .arangoicon", "top");

    return this;
  },
  showLoadingState: function () {
    $('.dataTables_empty').text('Loading...');
  },
  renderPagination: function (totalPages, filter) {

    var checkFilter = filter;
    var self = this;

    var currentPage;
    if (checkFilter === true) {
      currentPage = window.arangoDocumentsStore.currentFilterPage;
    }
    else {
      currentPage = JSON.parse(this.pageid);
    }
    var target = $('#documentsToolbarF'),
    options = {
      left: 2,
      right: 2,
      page: currentPage,
      lastPage: totalPages,
      click: function(i) {
        options.page = i;
        if (checkFilter === true) {

          var filterArray = self.getFilterContent();
          var filters = filterArray[0];
          var bindValues = filterArray[1];
          self.addDocumentSwitch = false;

          window.documentsView.clearTable();
          window.arangoDocumentsStore.getFilteredDocuments(self.colid, i, filters, bindValues);

          //Hide first/last pagination
          $('#documents_last').css("visibility", "hidden");
          $('#documents_first').css("visibility", "hidden");
        }
        else {
          var windowLocationHash =  '#collection/' + self.colid + '/documents/' + options.page;
          window.location.hash = windowLocationHash;
        }
      }
    };
    target.pagination(options);
    $('#documentsToolbarF').prepend(
      '<ul class="prePagi"><li><a id="documents_first">'+
      '<span class="glyphicon glyphicon-step-backward"></span></a></li></ul>');
      $('#documentsToolbarF').append(
        '<ul class="lasPagi"><li><a id="documents_last">'+
        '<span class="glyphicon glyphicon-step-forward"></span></a></li></ul>');
        var total = $('#totalDocuments');
        if (total.length > 0) {
          total.html("Total: " + this.documentsCount + " documents");
        } else {
          $('#documentsToolbarFL').append(
            '<a id="totalDocuments">Total: ' + this.documentsCount + ' document(s) </a>'
          );
        }
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
  createIndex: function (e) {
    //e.preventDefault();
    var self = this;
    var collection = this.collectionName;
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
    result = window.arangoCollectionsStore.createIndex(collection, postParameter);
    if (result === true) {
      $('#collectionEditIndexTable tr').remove();
      self.getIndex();
      self.toggleNewIndexView();
      self.resetIndexForms();
    }
    else {
      if (result.responseText) {
        var message = JSON.parse(result.responseText);
        arangoHelper.arangoNotification(message.errorMessage);
      }
      else {
        arangoHelper.arangoNotification("Could not create index.");
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
    var result = window.arangoCollectionsStore.deleteIndex(this.collectionName, this.lastId);
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
    var checked = $(id).is('checked');
    return checked;
  },
  getIndex: function () {
    this.index = window.arangoCollectionsStore.getIndex(this.collectionID, true);
    var cssClass = 'collectionInfoTh modal-text';
    if (this.index) {
      var fieldString = '';
      var indexId = '';
      var actionString = '';

      $.each(this.index.indexes, function(k,v) {
        if (v.type === 'primary' || v.type === 'edge') {
          actionString = '<span class="glyphicon glyphicon-ban-circle" ' +
            'data-original-title="No action"></span>';
        }
        else {
          actionString = '<span class="deleteIndex glyphicon glyphicon-minus-sign" ' +
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
