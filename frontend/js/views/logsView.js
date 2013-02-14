var logsView = Backbone.View.extend({
  el: '#content',
  offset: 0,
  size: 10,
  page: 1,
  table: 'logTableID',
  totalAmount: 0,
  totalPages: 0,

  initialize: function () {
    this.totalAmount = this.collection.models[0].attributes.totalAmount;
    this.totalPages = Math.ceil(this.totalAmount / this.size);
  },

  events: {
    "click #all-switch" : "all",
    "click #error-switch" : "error",
    "click #warning-switch" : "warning",
    "click #debug-switch" : "debug",
    "click #info-switch" : "info",
    "click #logTableID_first" : "firstTable",
    "click #logTableID_last"  : "lastTable",
    "click #logTableID_prev"  : "prevTable",
    "click #logTableID_next"  : "nextTable",
  },
  firstTable: function () {
    if (this.offset == 0) {
    }
    else {
      this.offset = 0;
      this.page = 1;
      this.clearTable();
      this.collection.fillLocalStorage(this.table, this.offset, this.size);
    }
  },
  lastTable: function () {
    if (this.page == this.totalPages) {
    }
    else {
      this.totalPages = Math.ceil(this.totalAmount / this.size);
      this.page = this.totalPages;
      this.offset = (this.totalPages * this.size) - this.size;
      this.clearTable();
      this.collection.fillLocalStorage(this.table, this.offset, this.size);
    }
  },
  prevTable: function () {
    if (this.offset == 0) {
    }
    else {
      this.offset = this.offset - this.size;
      this.page = this.page - 1;
      this.clearTable();
      this.collection.fillLocalStorage(this.table, this.offset, this.size);
    }
  },
  nextTable: function () {
    if (this.page == this.totalPages) {
    }
    else {
      this.page = this.page + 1;
      this.offset = this.offset + this.size;
      this.clearTable();
      this.collection.fillLocalStorage(this.table, this.offset, this.size);
    }
  },
  all: function () {
    this.resetState();
    this.table = "logTableID";
    this.clearTable();
    this.collection.fillLocalStorage(this.table, this.offset, this.size);
  },
  error: function() {
    this.resetState();
    this.table = "critTableID";
    this.clearTable();
    this.collection.fillLocalStorage(this.table, this.offset, this.size);
  },
  warning: function() {
    this.resetState();
    this.table = "warnTableID";
    this.clearTable();
    this.collection.fillLocalStorage(this.table, this.offset, this.size);
  },
  debug: function() {
    this.resetState();
    this.table = "debugTableID";
    this.clearTable();
    this.collection.fillLocalStorage(this.table, 0, this.size);
  },
  info: function() {
    this.resetState();
    this.table = "infoTableID";
    this.clearTable();
    this.collection.fillLocalStorage(this.table, 0, this.size);
  },
  resetState: function () {
    this.offset = 0;
    this.size = 10;
    this.page = 1;
  },

  tabs: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/logsView.ejs'}),
  initLogTables: function () {
    var self = this;
    $.each(this.collection.tables, function(key, table) {
      table = $('#'+table).dataTable({
        "bFilter": false,
        "bPaginate": false,
        "bLengthChange": false,
        "bDeferRender": true,
        "bProcessing": true,
        "bAutoWidth": false,
        "iDisplayLength": -1,
        "bJQueryUI": false,
        "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"sWidth": "800px","bSortable":false, "sClass":"logContent"}],
        "oLanguage": {"sEmptyTable": "No logfiles available"}
      });
    });

  },
  render: function() {
    $(this.el).html(this.template.text);
    return this;
  },
  drawTable: function () {
    var self = this;
    $.each(window.arangoLogsStore.models, function(key, value) {
      $('#'+self.table).dataTable().fnAddData([value.attributes.level, value.attributes.text]);
    });
  },
  clearTable: function () {
    $('#'+this.table).dataTable().fnClearTable();
  }
});
