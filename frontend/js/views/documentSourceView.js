var documentSourceView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },
  events: {
    "click #tableView"     :   "tableView",
    "click #saveSourceDoc" :   "saveSourceDoc",
  },

  template: new EJS({url: '/_admin/html/js/templates/documentSourceView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    this.breadcrumb();
    this.editor();
    return this;
  },
  editor: function () {
    var editor = ace.edit("sourceEditor");
    editor.getSession().setMode("ace/mode/javascript");
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
  saveSourceDoc: function() {
    window.arangoDocumentStore.saveDocument("source");
  },
  tableView: function () {
    var hash = window.location.hash.split("/");
    window.location.hash = hash[0]+"/"+hash[1]+"/"+hash[2];
  },
  fillSourceBox: function () {
    var data = arangoDocumentStore.models[0].attributes;
    var model = [];
    $.each(data, function(key, val) {
      if (arangoHelper.isSystemAttribute(key) === true) {
        delete data[key];
      }
    });
    var editor = ace.edit("sourceEditor");
    editor.setValue(this.FormatJSON(data));
  },
  stateReplace: function (value) {
    var inString = false;
    var length = value.length;
    var position = 0;
    var escaped = false;

    var output = "";
    while (position < length) {
      var c = value.charAt(position++);

      if (c === '\\') {
        if (escaped) {
          /* case: \ followed by \ */
          output += '\\\\';
          escaped = false;
        }
        else {
          /* case: single backslash */
          escaped = true;
        }
      }
      else if (c === '"') {
        if (escaped) {
          /* case: \ followed by " */
          output += '\\"';
          escaped = false;
        }
        else {
          output += '"';
          inString = !inString;
        }
      }
      else {
        if (inString) {
          if (escaped) {
            output += '\\' + c;
            escaped = false;
          }
          else {
            switch (c) {
              case '\b':
                output += '\\b';
              break;
              case '\f':
                output += '\\f';
              break;
              case '\n':
                output += '\\n';
              break;
              case '\t':
                output += '\\t';
              break;
              case '\r':
                output += '\\r';
              break;
              default:
                output += c;
              break;
            }
          }
        }
        else {
          if (c >= '!') {
            output += c;
          }
        }
      }
    }

    return output;
  },
  FormatJSON: function (oData, sIndent) {
    var self = this;
    if (arguments.length < 2) {
      var sIndent = "";
    }
    var sIndentStyle = " ";
    var sDataType = self.RealTypeOf(oData);

    if (sDataType == "array") {
      if (oData.length == 0) {
        return "[]";
      }
      var sHTML = "[";
    } else {
      var iCount = 0;
      $.each(oData, function() {
        iCount++;
        return;
      });
      if (iCount == 0) { // object is empty
        return "{}";
      }
      var sHTML = "{";
    }

    var iCount = 0;
    $.each(oData, function(sKey, vValue) {
      if (iCount > 0) {
        sHTML += ",";
      }
      if (sDataType == "array") {
        sHTML += ("\n" + sIndent + sIndentStyle);
      } else {
        sHTML += ("\n" + sIndent + sIndentStyle + JSON.stringify(sKey) + ": ");
      }

      // display relevant data type
      switch (self.RealTypeOf(vValue)) {
        case "array":
          case "object":
          sHTML += self.FormatJSON(vValue, (sIndent + sIndentStyle));
        break;
        case "boolean":
          case "number":
          sHTML += vValue.toString();
        break;
        case "null":
          sHTML += "null";
        break;
        case "string":
          sHTML += "\"" + vValue.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"";
        break;
        default:
          sHTML += ("TYPEOF: " + typeof(vValue));
      }

      // loop
      iCount++;
    });

    // close object
    if (sDataType == "array") {
      sHTML += ("\n" + sIndent + "]");
    } else {
      sHTML += ("\n" + sIndent + "}");
    }

    // return
    return sHTML;
  },
  RealTypeOf: function (v) {
    if (typeof(v) == "object") {
      if (v === null) return "null";
      if (v.constructor == (new Array).constructor) return "array";
      if (v.constructor == (new Date).constructor) return "date";
      if (v.constructor == (new RegExp).constructor) return "regex";
      return "object";
    }
    return typeof(v);
  }

});
