var documentView = Backbone.View.extend({
  el: '#content',
  table: '#documentTableID',
  colid: 0,
  docid: 0,
  currentKey: 0,

  init: function () {
    this.initTable();
  },

  events: {
    "click #saveDocument"       : "saveDocument",
    //"click #addDocumentLine"    : "addLine",
    "click #deleteRow"          : "deleteLine",
    "click #sourceView"         : "sourceView",
    "click #editFirstRow"       : "editFirst",
    "click #documentTableID tr" : "clicked",
    "click #editSecondRow"      : "editSecond",
    "click .cancelButton"       : "checkIfEmpty",
    "keydown .sorting_1"        : "listenKey",
    "keydown"                   : "listenGlobalKey",
    "focusout textarea"         : "checkFocus"
  },

  checkFocus: function(e) {
    //check if new row has to be saved
    var self = this;
    var data = $(this.table).dataTable().fnGetData();
    $.each(data, function(key, val) {
      if (val[0] === self.currentKey) {
        $(self.table).dataTable().fnDeleteRow( key );
      }
    });

  },

  listenGlobalKey: function(e) {
    if (e.keyCode === 27) {
      this.checkFocus();
    }
  },

  listenKey: function(e) {
    if (e.keyCode === 13) {
      console.log('enter');
      $('.btn-success').click();
    }
  },

  template: new EJS({url: '/_admin/html/js/templates/documentView.ejs'}),

  typeCheck: function (type) {
    if (type === 'edge') {
      var result = window.arangoDocumentStore.getEdge(this.colid, this.docid);
      if (result === true) {
        this.initTable();
        this.drawTable();
      }
      else if (result === false) {
      }
    }
    else if (type === 'document') {
      var result = window.arangoDocumentStore.getDocument(this.colid, this.docid);
      if (result === true) {
        this.initTable();
        this.drawTable();
      }
      else if (result === false) {
      }
    }
  },
  clicked: function (a) {
    self = a.currentTarget;
    var checkData = $(this.table).dataTable().fnGetData(self);
    try {
      if (checkData[0] === '<div class="notwriteable"></div>') {
        this.addLine();
        return;
      }
    }
    catch (e) {
      return;
    }
  },
  render: function() {
    $(this.el).html(this.template.text);
    this.breadcrumb();
    return this;
  },
  editFirst: function (e) {
    var element = e.currentTarget;
    var prevElement = $(element).parent().prev();
    $(prevElement).click();
  },
  editSecond: function (e) {
    var element = e.currentTarget;
    var prevElement = $(element).parent().prev();
    $(prevElement).click();
  },
  sourceView: function () {
    window.location.hash = window.location.hash + "/source";
  },
  saveDocument: function () {
    if (this.type === 'document') {
      var model = window.arangoDocumentStore.models[0].attributes;
      model = JSON.stringify(model);
      var result = window.arangoDocumentStore.saveDocument(this.colid, this.docid, model);
      if (result === true) {
        arangoHelper.arangoNotification('Document saved');
      }
      else if (result === false) {
        arangoHelper.arangoAlert('Document error');
      }
    }
    else if (this.type === 'edge') {
      var model = window.arangoDocumentStore.models[0].attributes;
      model = JSON.stringify(model);
      var result = window.arangoDocumentStore.saveEdge(this.colid, this.docid, model);
      if (result === true) {
        arangoHelper.arangoNotification('Edge saved');
      }
      else if (result === false) {
        arangoHelper.arangoError('Edge error');
      }
    }
  },
  breadcrumb: function () {
    var name = window.location.hash.split("/");
    $('#transparentHeader').append(
      '<div class="breadcrumb">'+
      '<a href="#" class="activeBread">Collections</a>'+
      '  >  '+
      '<a class="activeBread" href="#collection/'+name[1]+'/documents/1">'+name[1]+'</a>'+
      '  >  '+
      '<a class="disabledBread">'+name[2]+'</a>'+
      '</div>'
    );
  },
  drawTable: function () {
    var self = this;
    $(self.table).dataTable().fnAddData([
      '<div class="notwriteable"></div>',
      '<div class="notwriteable"></div>',
      '<a class="add" class="notwriteable" id="addDocumentLine"> </a>',
      '<div class="notwriteable"></div>',
      '<div class="notwriteable"></div>',
      '<button class="enabled" id="addRow"><img id="addDocumentLine" class="plusIcon" src="/_admin/html/img/plus_icon.png"></button>'
    ]);
    $.each(window.arangoDocumentStore.models[0].attributes, function(key, value) {
      if (arangoHelper.isSystemAttribute(key)) {
        $(self.table).dataTable().fnAddData([
          key,
          '',
          self.value2html(value, true),
          JSON.stringify(value),
          "",
          ""
        ]);
      }
      else {
        $(self.table).dataTable().fnAddData(
          [
            key,
            '<i class="icon-edit" id="editFirstRow"></i>',
            self.value2html(value),
            JSON.stringify(value),
            '<i class="icon-edit" id="editSecondRow"></i>',
            '<button class="enabled" id="deleteRow"><img src="/_admin/html/img/icon_delete.png" width="16" height="16"></button>'
        ]);
      }
    });
    this.makeEditable();
    $(this.table).dataTable().fnSort([ [0,'asc'] ]);

  },

  addLine: function () {
    var randomKey = arangoHelper.getRandomToken();
    var self = this;
    self.currentKey = "zkey"+randomKey;
    $(this.table).dataTable().fnAddData(
      [
        self.currentKey,
        '<i class="icon-edit" id="editFirstRow"></i>',
        this.value2html("editme"),
        JSON.stringify("editme"),
        '<i class="icon-edit" id="editSecondRow"></i>',
        '<button class="enabled" id="deleteRow"><img src="/_admin/html/img/icon_delete.png" width="16" height="16"></button>'
      ]
    );
    this.makeEditable();
    var tableContent = $('table').find('td');
    $.each(tableContent, function(key, val) {
      if ($(val).text() === self.currentKey) {
        $(val).click();
        $('.jediTextarea textarea').val("");
        return;
      }
    });
  },

  deleteLine: function (a) {
    var row = $(a.currentTarget).closest("tr").get(0);
    $(this.table).dataTable().fnDeleteRow($(this.table).dataTable().fnGetPosition(row));
    this.updateLocalDocumentStorage();
  },

  initTable: function () {
    var documentTable = $(this.table).dataTable({
      "aaSorting": [[ 1, "asc" ]],
      "bAutoWidth": false,
      "bFilter": false,
      "bPaginate":false,
      "bSortable": false,
      "bLengthChange": false,
      "bDeferRender": true,
      "iDisplayLength": -1,
      "aoColumns": [
        {"sClass":"writeable", "bSortable": false, "sWidth":"200px" },
        {"sClass":"read_only leftCell", "bSortable": false, "sWidth": "20px"},
        {"sClass":"writeable rightCell", "bSortable": false},
        {"bVisible": false },
        {"sClass":"read_only leftCell", "bSortable": false, "sWidth": "20px"},
        {"sClass":"read_only leftCell", "bSortable": false, "sWidth": "30px"}
      ],
      "oLanguage": {"sEmptyTable": "No documents"}
    });
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
      //Exclude "add-collection" row
      if (row !== '0') {
        var row_data = data[row];
        result[row_data[0]] = JSON.parse(row_data[3]);
      }
    }
    window.arangoDocumentStore.updateLocalDocument(result);
    //then sent to server
    this.saveDocument();
  },
  makeEditable: function () {
    var documentEditTable = $(this.table).dataTable();
    var self=this;
    var i = 0;
    $('.writeable', documentEditTable.fnGetNodes() ).each(function () {
      var aPos = documentEditTable.fnGetPosition(this);
      if (aPos[0] === 0) {
        $(this).removeClass('writeable');
      }
      if ( i == 1) {
        $(this).removeClass('writeable');
        i = 0;
      }
      if (arangoHelper.isSystemAttribute(this.innerHTML)) {
        $(this).removeClass('writeable');
        i = 1;
      }
    });
    $('.writeable', documentEditTable.fnGetNodes()).editable(function(value, settings) {
      var aPos = documentEditTable.fnGetPosition(this);
      if (aPos[1] == 0) {
          documentEditTable.fnUpdate(self.escaped(value), aPos[0], aPos[1]);
          self.updateLocalDocumentStorage();
          return value;
      }
      if (aPos[1] == 2) {
        var oldContent = JSON.parse(documentEditTable.fnGetData(aPos[0], aPos[1] + 1));
        var test = self.getTypedValue(value);
        if (String(value) == String(oldContent)) {
          // no change
          return self.value2html(oldContent);
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
        if (aPos[1] == 0) {
        //check if this row was newly created
        if (value === self.currentKey) {
          console.log("ist alt"); 
          return value;
        }
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
      submitcssclass: 'btn btn-success pull-right',
      cancelcssclass: 'btn btn-danger pull-right',
      cancel: '<button class="cancelButton btn btn-danger pull-right">Cancel</button">',
      submit: 'Save',
      onblur: 'cancel',
      autogrow: {lineHeight: 16, minHeight: 30}
      //style: 'display: inline',
    });
  },
  checkIfEmpty: function (e) {
    console.log(e);
    console.log("TEST");
    var test = $('.jediTextarea button');
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
      arangoHelper.arangoNotification('You have entered an invalid string value. Please review and adjust it.');
      throw "error";
    }

    try {
      value = JSON.parse(value);
    }
    catch (e) {
      arangoHelper.arangoNotification('You have entered an invalid string value. Please review and adjust it.');
      throw e;
    }
    return value;
  }


});
