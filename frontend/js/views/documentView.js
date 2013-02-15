var documentView = Backbone.View.extend({
  el: '#content',
  table: '#documentTableID',
  counter: 1,
  init: function () {
    this.initTable();
  },

  events: {
    "click #saveDocument"    : "saveDocument",
    "click #addDocumentLine" : "addLine",
    "click #deleteRow"       : "deleteLine",
    "click #sourceView"      : "sourceView"
  },

  template: new EJS({url: '/_admin/html/js/templates/documentView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
  },
  sourceView: function () {
    window.location.hash = window.location.hash + "/source";
  },
  saveDocument: function () {
    window.arangoDocumentStore.saveDocument();
  },

  drawTable: function () {
    var self = this;
    $.each(window.arangoDocumentStore.models[0].attributes, function(key, value) {
      if (self.isSystemAttribute(key)) {
        $(self.table).dataTable().fnAddData(["", key, self.value2html(value, true), JSON.stringify(value)]);
      }
      else {
        $(self.table).dataTable().fnAddData(['<button class="enabled" id="deleteRow"><img src="/_admin/html/img/delete_icon16.png" width="16" height="16"></button>',key, self.value2html(value), JSON.stringify(value)]);
      }
    });
    this.makeEditable();
  },

  addLine: function () {
    $(this.table).dataTable().fnAddData(['<button class="enabled" id="deleteRow"><img src="/_admin/html/img/delete_icon16.png" width="16" height="16"></button>', "somekey"+this.counter, this.value2html("editme"), JSON.stringify("editme")]);
    this.makeEditable();
    this.updateLocalDocumentStorage();
    this.counter++;
  },

  deleteLine: function (a) {
    var row = $(a.currentTarget).closest("tr").get(0);
    $(this.table).dataTable().fnDeleteRow($(this.table).dataTable().fnGetPosition(row));
    this.updateLocalDocumentStorage();
  },

  initTable: function () {
    var documentTable = $(this.table).dataTable({
      "aaSorting": [[ 1, "desc" ]],
      "bAutoWidth": false,
      "bFilter": false,
      "bPaginate":false,
      "bSortable": false,
      "bLengthChange": false,
      "bDeferRender": true,
      "iDisplayLength": -1,
      "aoColumns": [
        {"sClass":"read_only leftCell", "bSortable": false, "sWidth": "30px"},
        {"sClass":"writeable", "bSortable": false, "sWidth":"400px" },
        {"sClass":"writeable rightCell", "bSortable": false},
        {"bVisible": false }
      ],
      "oLanguage": {"sEmptyTable": "No documents"}
    });
  },

  systemAttributes: function () {
    return {
      '_id' : true,
      '_rev' : true,
      '_key' : true,
      '_from' : true,
      '_to' : true,
      '_bidirectional' : true,
      '_vertices' : true,
      '_from' : true,
      '_to' : true,
      '$id' : true
    };
  },

  isSystemAttribute: function (val) {
    var a = this.systemAttributes();
    return a[val];
  },

  isSystemCollection: function (val) {
    return val && val.name && val.name.substr(0, 1) === '_';
  },

  value2html: function (value, isReadOnly) {
    var self = this;
    var typify = function (value) {
      var checked = typeof(value);
      switch(checked) {
        case 'number':
          return ("<a class=\"sh_number\">" + value + "</a>");
        case 'string':
          return ("<a class=\"sh_string\">" + self.escaped(value) + "</a>");
        case 'boolean':
          return ("<a class=\"sh_keyword\">" + value + "</a>");
        case 'object':
          if (value instanceof Array) {
          return ("<a class=\"sh_array\">" + self.escaped(JSON.stringify(value)) + "</a>");
        }
        else {
          return ("<a class=\"sh_object\">"+ self.escaped(JSON.stringify(value)) + "</a>");
        }
      }
    };
    return (isReadOnly ? "(read-only) " : "") + typify(value);
  },

  escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#38;");
  },

  updateLocalDocumentStorage: function () {
    var data = $(this.table).dataTable().fnGetData();
    var result = {};

    for (row in data) {
      var row_data = data[row];
      result[row_data[1]] = JSON.parse(row_data[3]);
    }
    window.arangoDocumentStore.updateLocalDocument(result);
  },

  makeEditable: function () {
    var documentEditTable = $(this.table).dataTable();
    var self=this;
    var i = 0;
    $('.writeable', documentEditTable.fnGetNodes() ).each(function () {
      if ( i == 1) {
        $(this).removeClass('writeable');
        i = 0;
      }
      if (self.isSystemAttribute(this.innerHTML)) {
        $(this).removeClass('writeable');
        i = 1;
      }
    });
    $('.writeable', documentEditTable.fnGetNodes()).editable(function(value, settings) {
      var aPos = documentEditTable.fnGetPosition(this);
      if (aPos[1] == 1) {
        documentEditTable.fnUpdate(value, aPos[0], aPos[1]);
        self.updateLocalDocumentStorage();
        return value;
      }
      if (aPos[1] == 2) {
        var oldContent = JSON.parse(documentEditTable.fnGetData(aPos[0], aPos[1] + 1));
        var test = self.getTypedValue(value);
        if (String(value) == String(oldContent)) {
          // no change
          return value2html(oldContent);
        }
        else {
          // change update hidden row
          documentEditTable.fnUpdate(JSON.stringify(test), aPos[0], aPos[1] + 1);
          self.updateLocalDocumentStorage();
          // return visible row
          return self.value2html(test);
        }
      }
    },{
      data: function() {
        var aPos = documentEditTable.fnGetPosition(this);
        var value = documentEditTable.fnGetData(aPos[0], aPos[1]);

        if (aPos[1] == 1) {
          return value;
        }
        if (aPos[1] == 2) {
          var oldContent = documentEditTable.fnGetData(aPos[0], aPos[1] + 1);
          if (typeof(oldContent) == 'object') {
            //grep hidden row and paste in visible row
            return value2html(oldContent);
          }
          else {
            return oldContent;
          }
        }
      },
      //type: 'textarea',
      type: "autogrow",
      tooltip: 'click to edit',
      cssclass : 'jediTextarea',
      submit: 'Okay',
      cancel: 'Cancel',
      onblur: 'cancel',
      //style: 'display: inline',
      autogrow: {lineHeight: 16, minHeight: 30}
    });
  },
  getTypedValue: function (value) {
    value = value.replace(/(^\s+|\s+$)/g, '');
    if (value == 'true') {
      return true;
    }
    if (value == 'false') {
      return false;
    }
    if (value == 'null') {
      return null;
    }
    if (value.match(/^-?((\d+)?\.)?\d+$/)) {
      // TODO: support exp notation
      return parseFloat(value);
    }

    try {
      // assume list or object
      var test = JSON.parse(value);
      if (test instanceof Array) {
        // value is an array
        return test;
      }
      if (typeof(test) == 'object') {
        // value is an object 
        return test;
      }
    }
    catch (err) {
    }

    // fallback: value is a string
    value = value + '';

    if (value !== '' && (value.substr(0, 1) != '"' || value.substr(-1) != '"')) {
      alert("You have entered an invalid string value. Please review and adjust it.");
      throw "error";
    }

    try {
      value = JSON.parse(value);
    }
    catch (e) {
      alert("You have entered an invalid string value. Please review and adjust it.");
      throw e;
    }
    return value;
  }


});
