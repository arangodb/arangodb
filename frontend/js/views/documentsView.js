var documentsView = Backbone.View.extend({
  collectionID: 0,
  currentPage: 1,
  documentsPerPage: 10,
  totalPages: 1,

  alreadyClicked: false,

  el: '#content',
  table: '#documentsTableID',

  events: {
    "click #documentsTableID tr" : "clicked",
    "click #deleteDoc"           : "remove",
    "click #plusIcon"            : "addDocument",
    "click #documents_first"     : "firstDocuments",
    "click #documents_last"      : "lastDocuments",
    "click #documents_prev"      : "prevDocuments",
    "click #documents_next"      : "nextDocuments",
    "click #confirmDeleteBtn"    : "confirmDelete"
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
        { "bSortable": false, "sClass": "cuttedContent rightCell", "sWidth": "500px"}
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
										'<a href="#new" class="add"><img id="newCollection" src="/_admin/html/img/plus_icon.png"class="pull-left"></img> Neu hinzuf&uuml;gen</a>',
										''
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
    $(this.el).html(this.template.text);
    this.breadcrumb();
    return this;
  },
  breadcrumb: function () {
    var name = window.location.hash.split("/")[1];
    $('#transparentHeader').append(
      '<div class="breadcrumb">'+
      '<a class="activeBread" href="#">Collections</a>'+
      '  >  '+
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
