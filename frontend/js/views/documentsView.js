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

    "click #documentsTableID tr" : "clicked",
    "click #deleteDoc"           : "remove",
    "click #plusIconDoc"         : "addDocument",
    "click #documentAddBtn"      : "addDocument",
    "click #documents_first"     : "firstDocuments",
    "click #documents_last"      : "lastDocuments",
    "click #documents_prev"      : "prevDocuments",
    "click #documents_next"      : "nextDocuments",
    "click #confirmDeleteBtn"    : "confirmDelete"
  },

  buildCollectionLink : function (collection) {
    return "#collection/" + encodeURIComponent(collection.get('name')) + '/documents/1';
  },

  prevCollection : function () {
    if (this.collectionContext.prev !== null) {
      $('#collectionPrev').parent().removeClass('disabledPag');
      window.location.hash = this.buildCollectionLink(this.collectionContext.prev);
    }
    else {
      $('#collectionPrev').parent().addClass('disabledPag');
    }
  },

  nextCollection : function () {
    if (this.collectionContext.next !== null) {
      $('#collectionNext').parent().removeClass('disabledPag');
      window.location.hash = this.buildCollectionLink(this.collectionContext.next);
    }
    else {
      $('#collectionNext').parent().addClass('disabledPag');
    }
  },

  addDocument: function () {
    var collid  = window.location.hash.split("/")[1];
    window.arangoDocumentStore.addDocument(collid);
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
    var todelete = $(self.idelement).text();
    var deleted = window.arangoDocumentStore.deleteDocument(todelete);
    if (deleted === true) {
      var row = $(self.target).closest("tr").get(0);
      $('#documentsTableID').dataTable().fnDeleteRow($('#documentsTableID').dataTable().fnGetPosition(row));
      var hash = window.location.hash.split("/");
      var page = hash[3];
      var collection = hash[1];
      //TODO: more elegant solution...
      $('#documentsTableID').dataTable().fnClearTable();
      window.arangoDocumentsStore.getDocuments(collection, page);
      $('#docDeleteModal').modal('hide');
    }
    else {
      alert("something wrong");
      $('#docDeleteModal').modal('hide');
    }
  },
  clicked: function (a) {
    if (this.alreadyClicked == true) {
      this.alreadyClicked = false;
      return 0;
    }
    var self = a.currentTarget;
    var aPos = $(this.table).dataTable().fnGetPosition(self);

    var checkData = $(this.table).dataTable().fnGetData(self);
    if (checkData[0] === '') {
      this.addDocument();
      return;
    }

    var rowContent = $(this.table).dataTable().fnGetData(aPos);
    window.location.hash = "#collection/" + rowContent[0];
  },

  initTable: function (colid, pageid) {
    this.collectionID = colid;
    this.currentPage = pageid;
    var documentsTable = $('#documentsTableID').dataTable({
      "aaSorting": [[ 1, "asc" ]],
      "bFilter": false,
      "bPaginate":false,
      "bSortable": false,
      "bSort": false,
      "bLengthChange": false,
      "bAutoWidth": false,
      "iDisplayLength": -1,
      "bJQueryUI": false,
      "aoColumns": [
        { "sClass":"read_only leftCell docleftico", "bSortable": false, "sWidth":"30px"},
        { "sClass":"read_only arangoTooltip","bSortable": false},
        { "bSortable": false, "sClass": "cuttedContent rightCell"}
      ],
      "oLanguage": { "sEmptyTable": "No documents"}
    });
  },
  clearTable: function() {
    $(this.table).dataTable().fnClearTable();
  },
  drawTable: function() {
    var self = this;
    $.each(window.arangoDocumentsStore.models, function(key, value) {
      $(self.table).dataTable().fnAddData([
                                          value.attributes.id,
                                          //value.attributes.key,
                                          //value.attributes.rev,
                                          '<pre class=prettify title="'+self.escaped(JSON.stringify(value.attributes.content)) +'">' + self.cutByResolution(JSON.stringify(value.attributes.content)) + '</pre>',
                                          '<button class="enabled" id="deleteDoc"><img src="/_admin/html/img/icon_delete.png" width="16" height="16"></button>'
      ]);
    });
	$(self.table).dataTable().fnAddData([
										'',
										'<a id="plusIconDoc" style="padding-left: 30px">Add document</a>',
										'<img src="/_admin/html/img/plus_icon.png" id="documentAddBtn"></img>'
		]);
    $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
    $(".prettify").tooltip({
      placement: "top"
    });
    this.totalPages = window.arangoDocumentsStore.totalPages;
    this.currentPage = window.arangoDocumentsStore.currentPage;
    this.documentsCount = window.arangoDocumentsStore.documentsCount;

    if (this.documentsCount === 0) {
      $('#documentsToolbarLeft').html('No documents');
    }
    else {
      $('#documentsToolbarLeft').html(
        'Showing Page '+this.currentPage+' of '+this.totalPages+
        ', '+this.documentsCount+' entries'
      );
    }
  },

  template: new EJS({url: '/_admin/html/js/templates/documentsView.ejs'}),

  render: function() {
    this.collectionContext = window.arangoCollectionsStore.getPosition(this.colid);

    $(this.el).html(this.template.text);
    this.breadcrumb();
    if (this.collectionContext.prev === null) {
      $('#collectionPrev').parent().addClass('disabledPag');
    }
    if (this.collectionContext.next === null) {
      $('#collectionNext').parent().addClass('disabledPag');
    }
    return this;
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
    return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#38;");
  }

});
