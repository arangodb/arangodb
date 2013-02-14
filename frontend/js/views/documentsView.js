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
    "click #documents_first"     : "firstDocuments",
    "click #documents_last"      : "lastDocuments",
    "click #documents_prev"      : "prevDocuments",
    "click #documents_next"      : "nextDocuments"
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
    target = a.currentTarget;
    var thiselement = a.currentTarget.parentElement;
    var idelement = $(thiselement).next().text();
    this.alreadyClicked = true;

    //TODO: ALERT
    try {
      $.ajax({
        type: 'DELETE',
        contentType: "application/json",
        url: "/_api/document/" + idelement,
        success: function () {
          var row = $(target).closest("tr").get(0);
          $('#documentsTableID').dataTable().fnDeleteRow($('#documentsTableID').dataTable().fnGetPosition(row));
          var hash = window.location.hash.split("/");
          var page = hash[3];
          var collection = hash[1];

          //TODO: more elegant solution...
          $('#documentsTableID').dataTable().fnClearTable();
          window.arangoDocumentsStore.getDocuments(collection, page);
        }
      });
    }
    catch (e) {
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
    window.location.hash = "#collection/" + rowContent[1];
  },

  initTable: function (colid, pageid) {
    this.collectionID = colid;
    this.currentPage = pageid;
    var documentsTable = $('#documentsTableID').dataTable({
      "bFilter": false,
      "bPaginate":false,
      "bSortable": false,
      "bLengthChange": false,
      "bDeferRender": true,
      "bAutoWidth": false,
      "iDisplayLength": -1,
      "bJQueryUI": true,
      "aoColumns": [
        { "sClass":"read_only leftCell", "bSortable": false, "sWidth":"80px"},
        { "sClass":"read_only","bSortable": false, "sWidth": "200px"},
        { "sClass":"read_only","bSortable": false, "sWidth": "100px"},
        { "sClass":"read_only","bSortable": false, "sWidth": "100px"},
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
                                          '<button class="enabled" id="deleteDoc"><img src="/_admin/html/img/doc_delete_icon16.png" width="16" height="16"></button>',
                                          value.attributes.id,
                                          value.attributes.key,
                                          value.attributes.rev,
                                          '<pre class=prettify>' + self.cutByResolution(JSON.stringify(value.attributes.content)) + '</pre>'
      ]);
    });
    $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
  },

  template: new EJS({url: '/_admin/html/js/templates/documentsView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
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
