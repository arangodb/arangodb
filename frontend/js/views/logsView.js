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
  jumpToTable: function (toPage) {
      this.page = toPage;
      this.offset = toPage * this.size;
      this.clearTable();
      this.collection.fillLocalStorage(this.table, this.offset, this.size);
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
        "aoColumns": [
          { "sClass":"center firstcol", "bSortable":false},
          { "sClass":"center seccol", "bSortable":false},
          { "bSortable": false, "sClass":"logContent thirdcol"}
        ],
        "oLanguage": {"sEmptyTable": "No logfiles available"}
      });
    });

  },
  render: function() {
    $(this.el).html(this.template.text);
    $.gritter.removeAll();
    return this;
  },
  renderPagination: function (totalPages, currentPage) {
    var self = this;
    var target = $('#logtestdiv'),
    options = {
      left: 2,
      right: 2,
      page: currentPage,
      lastPage: totalPages,
      click: function(i) {
        if (i === 1 && i !== currentPage) {
          self.firstTable();
        }
        else if (i === totalPages && i !== currentPage) {
          self.lastTable();
        }
        else {
          self.jumpToTable(i);
        }
        options.page = i;
        target.pagination(options);
      }
    };
    target.pagination(options);
    $('#logtestdiv').prepend('<ul class="prePagi"><li><a id="logTableID_first"><i class="icon icon-step-backward"></i></a></li></ul>');
    $('#logtestdiv').append('<ul class="lasPagi"><li><a id="logTableID_last"><i class="icon icon-step-forward"></i></a></li></ul>');
  },
  drawTable: function () {
    var self = this;

    function format (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n
      };

      return dt.getUTCFullYear() + '-' 
      + pad(dt.getUTCMonth() + 1) + '-'
      + pad(dt.getUTCDate()) + ' <br>'
      + pad(dt.getUTCHours()) + ':'
      + pad(dt.getUTCMinutes()) + ':'
      + pad(dt.getUTCSeconds());
    }

    $.each(window.arangoLogsStore.models, function(key, value) {
      var convertedLog = self.convertLogStatus(value.attributes.level);
      var dt = new Date(value.attributes.timestamp * 1000);
      $('#'+self.table).dataTable().fnAddData([convertedLog, format(dt), value.attributes.text]);
    });
    try {
      this.totalAmount = this.collection.models[0].attributes.totalAmount;
      this.totalPages = Math.ceil(this.totalAmount / this.size);
      this.renderPagination(this.totalPages, this.page);
    }
    catch (e) {
      //  $('#logPages').html('No logfiles available');
    }
  },
  clearTable: function () {
    $('#'+this.table).dataTable().fnClearTable();
  },
  convertLogStatus: function (status) {
    if (status === 1) { return "Error" ;}
    else if (status === 2) { return "Warning" ;}
    else if (status === 3) { return "Info" ;}
    else if (status === 4) { return "Debug" ;}
    else { return "Unknown";}
  }
});
