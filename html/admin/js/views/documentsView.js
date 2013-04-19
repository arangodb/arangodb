var documentsView = Backbone.View.extend({
  collectionID: 0,
  currentPage: 1,
  documentsPerPage: 10,
  totalPages: 1,

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
    "click #confirmCreateEdge"   : "addEdge",
    "click #documentsTableID tr" : "clicked",
    "click #deleteDoc"           : "remove",
    "click #documents_first"     : "firstDocuments",
    "click #documents_last"      : "lastDocuments",
    "click #documents_prev"      : "prevDocuments",
    "click #documents_next"      : "nextDocuments",
    "click #confirmDeleteBtn"    : "confirmDelete",
    "keyup .modal-body"          : "listenKey",
    "click .key"                 : "nop"
  },

  nop: function(event) {
    event.stopPropagation();
  },

  listenKey: function (e) {
    if(e.keyCode === 13){
      this.addEdge();
    }
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
  addDocument: function () {
    var collid  = window.location.hash.split("/")[1];
    var doctype = arangoHelper.collectionApiType(collid);

    if (doctype === 'edge') {
      $('#edgeCreateModal').modal('show');
      $('.modalTooltips').tooltip({
        placement: "left"
      });
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

    $('#docDeleteModal').modal('show');

  },
  confirmDelete: function () {
    this.reallyDelete();
  },
  reallyDelete: function () {
    var self = this;
    var row = $(self.target).closest("tr").get(0);
    var hash = window.location.hash.split("/");
    var page = hash[3];
    var deleted = false;
    this.docid = $(self.idelement).next().text();

    if (this.type === 'document') {
      var result = window.arangoDocumentStore.deleteDocument(this.colid, this.docid);
      if (result === true) {
        //on success
        arangoHelper.arangoNotification('Document deleted');
        deleted = true;
      }
      else if (result === false) {
        arangoHelper.arangoError('Document error');
      }
    }
    else if (this.type === 'edge') {
      var result = window.arangoDocumentStore.deleteEdge(this.colid, this.docid);
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
      $('#documentsTableID').dataTable().fnDeleteRow($('#documentsTableID').dataTable().fnGetPosition(row));
      $('#documentsTableID').dataTable().fnClearTable();
      window.arangoDocumentsStore.getDocuments(this.colid, page);
      $('#docDeleteModal').modal('hide');
    }

  },
  clicked: function (event) {
    if (this.alreadyClicked == true) {
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
    this.collectionID = colid;
    this.currentPage = pageid;
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
        { "sClass":"","bSortable": false, "sWidth":"500px"},
        { "sClass":"", "bSortable": false, "sWidth":"30px"},
        { "bSortable": false, "sClass": "", "sWidth":"20px"}
      ],
      "oLanguage": { "sEmptyTable": "No documents"}
    });
  },
  clearTable: function() {
    $(this.table).dataTable().fnClearTable();
  },
  drawTable: function() {
    var self = this;

    $(self.table).dataTable().fnAddData([
      '<a id="plusIconDoc" style="padding-left: 30px">Add document</a>',
      '',
      '<img src="img/plus_icon.png" id="documentAddBtn"></img>'
    ]);

    $.each(window.arangoDocumentsStore.models, function(key, value) {

      var tempObj = {};
      $.each(value.attributes.content, function(k, v) {
        if (k === '_id' || k === '_rev' || k === '_key') {
        }
        else {
          tempObj[k] = v;
        }
      });

      $(self.table).dataTable().fnAddData([

        '<pre class="prettify" title="'
        + self.escaped(JSON.stringify(tempObj))
        + '">'
        + self.cutByResolution(JSON.stringify(tempObj))
        + '</pre>',

        '<div class="key">'
        + value.attributes.key
        + '</div>',

        '<button class="enabled" id="deleteDoc">'
        + '<img src="img/icon_delete.png" width="16" height="16"></button>'
      ]);
    });
    $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
/*    $(".prettify").tooltip({
      html: true,
      placement: "top"
    });*/
    this.totalPages = window.arangoDocumentsStore.totalPages;
    this.currentPage = window.arangoDocumentsStore.currentPage;
    this.documentsCount = window.arangoDocumentsStore.documentsCount;
    if (this.documentsCount === 0) {
    }
    else {
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
    this.initTable();
    this.breadcrumb();
    if (this.collectionContext.prev === null) {
      $('#collectionPrev').parent().addClass('disabledPag');
    }
    if (this.collectionContext.next === null) {
      $('#collectionNext').parent().addClass('disabledPag');
    }
    $.gritter.removeAll();

    return this;
  },
  renderPagination: function (totalPages) {
    var currentPage = JSON.parse(this.pageid);
    var self = this;
    var target = $('#documentsToolbarF'),
    options = {
      left: 2,
      right: 2,
      page: currentPage,
      lastPage: totalPages,
      click: function(i) {
        options.page = i;
        window.location.hash = '#collection/' + self.colid + '/documents/' + options.page;
      }
    };
    target.pagination(options);
    $('#documentsToolbarF').prepend('<ul class="prePagi"><li><a id="documents_first"><i class="icon icon-step-backward"></i></a></li></ul>');
    $('#documentsToolbarF').append('<ul class="lasPagi"><li><a id="documents_last"><i class="icon icon-step-forward"></i></a></li></ul>');
    var total = $('#totalDocuments');
    if (total.length > 0) {
      total.html("Total: " + this.documentsCount + " documents");
    } else {
      $('#documentsToolbarFL').append('<a id="totalDocuments">Total: ' + this.documentsCount + ' documents </a>');
    }
  },
  breadcrumb: function () {
    var name = window.location.hash.split("/")[1];
    $('#transparentHeader').append(
      '<div class="breadcrumb">'+
      '<a class="activeBread" href="#">Collections</a>'+
      '  &gt;  '+
      '<a class="disabledBread">'+name+'</a>'+
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
    return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
  }

});
