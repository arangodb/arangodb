/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true, forin: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, value2html, templateEngine */
/*global document */

(function() {
  "use strict";
  window.DocumentView = Backbone.View.extend({
    el: '#content',
    table: '#documentTableID',
    colid: 0,
    docid: 0,
    currentKey: 0,
    currentKeyState: "",
    oldDocumentState: "",
    documentCache: { },

    init: function () {
      this.initTable();

    },

    events: {
      "click #saveDocument"                     : "saveDocument",
      "click #addDocumentLine"                  : "addLine",
      "click .addAttribute"                     : "addLine",
      "click #documentTableID .deleteAttribute" : "deleteLine",
      "click #sourceView"                       : "sourceView",
      "click .editFirstAttribute"               : "editFirst",
      "click #documentTableID tr"               : "clicked",
      "click .editSecondAttribute"              : "editSecond",
      "keydown #documentviewMain"               : "listenGlobalKey",
      "blur #documentviewMain textarea"         : "checkFocus",
      "keydown #documentTableID tr td:first-child textarea" : "keyPressedTextareaLeft",
      "keydown #documentTableID tr .rightCell textarea" : "keyPressedTextareaRight"
    },

    keyPressedTextareaLeft: function(e) {
      //attributes inline-textarea-tab edit-mode
      //check if action is required (empty field)

      if (e.keyCode === 9) {
        if (!this.validateKey()) {
          e.preventDefault();
          return;
        }
        var altTarget = $(e.currentTarget).parent().parent().next().next();
        e.preventDefault();
        $('.btn-success').click();
        $(altTarget).click();
      }
      else if (e.keyCode === 13) {
        if (!this.validateKey()) {
          e.preventDefault();
          return;
        }
        $('.btn-success').click();
      }
    },

    validateKey: function () {
      var self = this;
      var returnval = true;
      var data = $('#documentTableID').dataTable().fnGetData();
      var focused = $('textarea').val();

      if (!focused.trim()) {
        arangoHelper.arangoNotification("Empty field");
        returnval = false;
      }

      var checkWhiteSpaces = focused.replace(/ /g,'');
      if (checkWhiteSpaces !== focused) {
        arangoHelper.arangoNotification("No whitespaces allowed");
        returnval = false;
      }

      $.each(data, function(key, val) {
        if (val[0] === focused) {
          if (val[0] === self.currentKeyState) {
            returnval = true;
          }
          else {
            arangoHelper.arangoNotification("Key already exists!");
            returnval = false;
          }
        }
      });
      return returnval;
    },

    keyPressedTextareaRight: function(e) {
      //values inline-textarea-tab edit-mode
      if (e.keyCode === 9) {
        e.preventDefault();
        var temp = $(e.currentTarget).parent().parent().parent().next().children();
        $('.btn-success').click();
        var altTarget = temp[0];
        while ($(altTarget).text() === '_id' ||
               $(altTarget).text() === '_rev' ||
               $(altTarget).text() === '_key') {
               var temp2 = $(altTarget).parent().next().children();
               altTarget = temp2[0];
        }
        $(altTarget).click();
      }
      else if (e.ctrlKey && e.keyCode === 13) {
        $('.btn-success').click();
      }
    },

    checkFocus: function(e) {
      //check if new row has to be saved
      var self = this;
      var data = $(this.table).dataTable().fnGetData();
      $.each(data, function(key, val) {

        var rowContent = $('.jediTextarea textarea').val();
        if (val[0] === self.currentKey && rowContent === '') {
          $(self.table).dataTable().fnDeleteRow( key );
          $('#addRow').removeClass('disabledBtn');
        }
      });
      $('td').removeClass('validateError');
      return true;
    },

    listenGlobalKey: function(e) {
      if (e.keyCode === 27) {
        this.checkFocus();
      }
    },

    template: templateEngine.createTemplate("documentView.ejs"),

    typeCheck: function (type) {
      var result;
      if (type === 'edge') {
        result = window.arangoDocumentStore.getEdge(this.colid, this.docid);
        if (result === true) {
          this.initTable();
          this.drawTable();
        }
      }
      else if (type === 'document') {
        result = window.arangoDocumentStore.getDocument(this.colid, this.docid);
        if (result === true) {
          this.initTable();
          this.drawTable();
        }
      }

      arangoHelper.fixTooltips(".icon_arangodb", "left");
      arangoHelper.fixTooltips(".docLink", "top");
    },
    clicked: function (a) {
      var self = a.currentTarget;
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
      var self = this;
      var model, result;
      model = window.arangoDocumentStore.models[0].attributes;
      model = JSON.stringify(model);

      //check if there are any changes, if not quit
      if (model === this.oldDocumentState) {
          $('.addAttribute').removeClass('disabledBtn');
        return;
      }

      if (this.type === 'document') {
        result = window.arangoDocumentStore.saveDocument(this.colid, this.docid, model);
        if (result === true) {
          arangoHelper.arangoNotification('Document saved');
          $('.addAttribute').removeClass('disabledBtn');
          $('td').removeClass('validateError');
          self.scrollToFocused();
        }
        else if (result === false) {
          arangoHelper.arangoAlert('Document error');
        }
      }
      else if (this.type === 'edge') {
        result = window.arangoDocumentStore.saveEdge(this.colid, this.docid, model);
        if (result === true) {
          arangoHelper.arangoNotification('Edge saved');
          $('.addAttribute').removeClass('disabledBtn');
          $('td').removeClass('validateError');
          self.scrollToFocused();
        }
        else if (result === false) {
          arangoHelper.arangoError('Edge error');
        }
      }
    },

    scrollToFocused: function () {
      //function to center focused element
      window.setTimeout(function() {
        var heightPosition = $(window).scrollTop();
        $(window).scrollTop(heightPosition-55);
      }, 150);
    },

    breadcrumb: function () {
      var name = window.location.hash.split("/");
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a href="#collections" class="activeBread">Collections</a>'+
        '  >  '+
        '<a class="activeBread" href="#collection/' + name[1] + '/documents/1">' + name[1] + '</a>'+
        '  >  '+
        '<a class="disabledBread">' + name[2] + '</a>'+
        '</div>'
      );
    },

    getLinkedDoc: function (handle) {
      var self = this;
      if (! self.documentCache.hasOwnProperty(handle)) {
        $.ajax({
          cache: false,
          type: "GET",
          async: false,
          url: "/_api/document/" + handle,
          contentType: "application/json",
          processData: false,
          success: function(data) {
            self.documentCache[handle] = data;
          },
          error: function(data) {
            self.documentCache[handle] = null; 
          }
        });
      }

      return self.documentCache[handle];
    },

    drawTable: function () {
      var self = this;
      $.each(window.arangoDocumentStore.models[0].attributes, function(key, value) {
        if (arangoHelper.isSystemAttribute(key)) {
          var preview = "";
          var html;

          if (key === "_from" || key === "_to") {
            var linkedDoc = self.getLinkedDoc(value);

            if (linkedDoc !== null && linkedDoc !== undefined) {
              preview = '<span class="docPreview arangoicon icon_arangodb_info" title="' +
                        self.escaped(JSON.stringify(linkedDoc)) + '"></span>';
              html = '<a href="#collection/' + value +
                     '" class="docLink" title="Go to document">' + self.escaped(value) +
                     '</a>';
            }
            else {
              html = self.escaped(value);
            }

          }
          else {
            html = self.value2html(value, true);
          }

          $(self.table).dataTable().fnAddData([
            key,
            preview,
            html,
            JSON.stringify(value, null, 4),
            "",
            ""
          ]);
        }
        else {
          $(self.table).dataTable().fnAddData([
            key,
            '<a class="editFirstAttribute"><span class="icon_arangodb_edit"></span></a>',
            self.value2html(value),
            JSON.stringify(value, null, 4),
            '<a class="editSecondAttribute"><span class="icon_arangodb_edit"></span></a>',
            '<a class="deleteAttribute"><span class="icon_arangodb_roundminus" ' +
            'title="Delete attribute"></span></a>'
          ]);
        }
      });
      this.makeEditable();

      $(document).bind('keydown', function(e) {
        if (e.ctrlKey && e.keyCode === 65 || e.ctrlKey && e.keyCode === 78) {
          if ($('#addNewRowEntry')) {
            $('#addNewRowEntry').click();
          }
        }
      });
    },

    jumpToPageBottom: function () {
      $('html, body').scrollTop($(document).height() - $(window).height());
    },

    addLine: function (event) {
      if ($('.addAttribute').hasClass('disabledBtn') === true) {
        return;
      }
      $('.addAttribute').addClass('disabledBtn');
      //event.stopPropagation();
      var randomKey = arangoHelper.getRandomToken();
      var self = this;
      self.currentKey = "zkey" + randomKey;
      $(this.table).dataTable().fnAddData(
        [
          self.currentKey,
          '<a class="editFirstAttribute"><span class="icon_arangodb_edit"></span></a>',
          this.value2html("editme"),
          JSON.stringify("editme"),
          '<a class="editSecondAttribute"><span class="icon_arangodb_edit"></span></a>',
          '<a class="deleteAttribute"><span class="icon_arangodb_roundminus" ' +
          'title="Delete attribute"></span></a>'
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

      self.jumpToPageBottom();
      arangoHelper.fixTooltips(".icon_arangodb", "left");
    },

    deleteLine: function (a) {
      var row = $(a.currentTarget).closest("tr").get(0);
      $(this.table).dataTable().fnDeleteRow($(this.table).dataTable().fnGetPosition(row));
      this.updateLocalDocumentStorage();
    },

    initTable: function () {
      $(this.table).dataTable({
        "bSortClasses": false,
        "bAutoWidth": false,
        "bFilter": false,
        "bPaginate":false,
        "bSortable": false,
        "bLengthChange": false,
        "bDeferRender": true,
        "iDisplayLength": -1,
        "aoColumns": [
          {"sClass":"writeable keyRow", "bSortable": false, "sWidth":"200px" },
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
        var checked = typeof value;
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
          return ("<a class=\"sh_object\">"+ self.escaped(JSON.stringify(value)) + "</a>");
        }
      };
      return (isReadOnly ? '<i class="readOnlyClass">' : '') + typify(value) + '</i>';
    },

    escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    },

    updateLocalDocumentStorage: function () {
      var data = $(this.table).dataTable().fnGetData();
      var result = {};
      var row;

      for (row in data) {
          var row_data = data[row];
          var key = row_data[0];
          var value = row_data[3];

          result[key] = JSON.parse(value);
      }
      window.arangoDocumentStore.updateLocalDocument(result);
      //then sent to server
      this.saveDocument();
    },
    makeEditable: function () {
      var documentEditTable = $(this.table).dataTable();
      var self = this;
      var i = 0;
      $('.writeable', documentEditTable.fnGetNodes() ).each(function () {
        if (i === 1) {
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
        if (aPos[1] === 0) {
          documentEditTable.fnUpdate(self.escaped(value), aPos[0], aPos[1]);
          self.updateLocalDocumentStorage();
          return value;
        }
        if (aPos[1] === 2) {
          var oldContent = JSON.parse(documentEditTable.fnGetData(aPos[0], aPos[1] + 1));
          var test = self.getTypedValue(value);
          if (String(value) === String(oldContent)) {
            // no change
            return self.value2html(oldContent);
          }
          // change update hidden row
          documentEditTable.fnUpdate(JSON.stringify(test), aPos[0], aPos[1] + 1);
          self.updateLocalDocumentStorage();
          // return visible row
          return self.value2html(test);
        }
      },{
        data: function() {
          $(".btn-success").click();
          var aPos = documentEditTable.fnGetPosition(this);
          var value = documentEditTable.fnGetData(aPos[0], aPos[1]);
          var model;
          if (aPos[1] === 0) {

            //save current document state
            model = window.arangoDocumentStore.models[0].attributes;
            self.oldDocumentState = JSON.stringify(model);
            //save current key state
            self.currentKeyState = value;

            //check if this row was newly created

            if (value === self.currentKey) {
              return value;
            }
            return value;
          }
          if (aPos[1] === 2) {
            var oldContent = documentEditTable.fnGetData(aPos[0], aPos[1] + 1);

            //save current document state
            model = window.arangoDocumentStore.models[0].attributes;
            self.oldDocumentState = JSON.stringify(model);

            if (typeof oldContent === 'object') {
              //grep hidden row and paste in visible row
              return value2html(oldContent);
            }
            return oldContent;
          }
        },
        width: "none", // if not set, each row will get bigger & bigger (Safari & Firefox)
        type: "autogrow",
        tooltip: 'click to edit',
        cssclass : 'jediTextarea',
        submitcssclass: 'btn btn-success pull-right',
        cancelcssclass: 'btn btn-danger pull-right',
        cancel: 'Cancel',
        submit: 'Save',
        onblur: 'cancel',
        onsubmit: self.validate,
        onreset: self.resetFunction
      });
    },
    resetFunction: function (settings, original) {
      try {
        var currentKey2 = window.documentView.currentKey;
        var data = $('#documentTableID').dataTable().fnGetData();
        $.each(data, function(key, val) {
          if (val[0] === currentKey2) {
            $('#documentTableID').dataTable().fnDeleteRow(key);
            $('.addAttribute').removeClass('disabledBtn');
          }
        });
      }
      catch (e) {
      }

    },

    //broken
    validate: function (settings, td) {
      var returnval = true;
      /*
      if ($(td).hasClass('keyRow') === true) {
        var toCheck = $('textarea').val();
        var data = $('#documentTableID').dataTable().fnGetData();

        if (toCheck === '') {
          $(td).addClass('validateError');
          arangoHelper.arangoNotification("Key is empty!");
          returnval = false;
          return returnval;
        }

        $.each(data, function(key, val) {
          if (val[0] === toCheck) {
            $(td).addClass('validateError');
            arangoHelper.arangoNotification("Key already exists!");
            returnval = false;
          }
        });
      }*/
      return returnval;
    },
    getTypedValue: function (value) {
      value = value.replace(/(^\s+|\s+$)/g, '');
      if (value === 'true') {
        return true;
      }
      if (value === 'false') {
        return false;
      }
      if (value === 'null') {
        return null;
      }
      if (value.match(/^-?((\d+)?\.)?\d+$/)) {
        //support exp notation
        return parseFloat(value);
      }

      try {
        // assume list or object
        var test = JSON.parse(value);
        if (test instanceof Array) {
          // value is an array
          return test;
        }
        if (typeof test === 'object') {
          // value is an object
          return test;
        }
      }
      catch (err) {
      }

      // fallback: value is a string
      value = String(value);

      if (value !== '' && (value.substr(0, 1) !== '"' || value.substr(-1) !== '"')) {
        arangoHelper.arangoNotification(
          'You have entered an invalid string value. Please review and adjust it.'
        );
        throw "error";
      }

      try {
        value = JSON.parse(value);
      }
      catch (e) {
        arangoHelper.arangoNotification(
          'You have entered an invalid string value. Please review and adjust it.'
        );
        throw e;
      }
      return value;
    }


  });
}());
