var queryView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },
  events: {
    'click #submitQueryIcon'   : 'submitQuery',
    'click #submitQueryButton' : 'submitQuery',
    'click .clearicon': 'clearOutput',
  },
  clearOutput: function() {
    $('#queryOutput').empty();
  },

  template: new EJS({url: '/_admin/html/js/templates/queryView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    var editor = ace.edit("aqlEditor");
    editor.getSession().setMode("ace/mode/javascript");
    editor.resize();
    $('#aqlEditor').focus();
    return this;
  },
  submitQuery: function() {
    var self = this;
    var editor = ace.edit("aqlEditor");
    var data = {query: editor.getValue()};

    $("#queryOutput").empty();
    $("#queryOutput").append('<pre class="preQuery">Loading...</pre>>'); 


    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        $("#queryOutput").empty();
        var formatQuestion = true;
        if (formatQuestion === true) {
          $("#queryOutput").append('<pre class="preQuery"><font color=green>' + self.FormatJSON(data.result) + '</font></pre>'); 
        }
        else {
          $("#queryOutput").append('<a class="querySuccess"><font color=green>' + self.JSON.stringify(data.result) + '</font></a>'); 
        }
      },
      error: function(data) {
        console.log(data);
        var temp = JSON.parse(data.responseText);
        $("#queryOutput").empty();
        $("#queryOutput").append('<a class="queryError"><font color=red>[' + temp.errorNum + '] ' + temp.errorMessage + '</font></a>'); 
      }
    });
  },
  FormatJSON: function(oData, sIndent) {
    var self = this;
    if (arguments.length < 2) {
      var sIndent = "";
    }
    var sIndentStyle = "    ";
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

    // loop through items
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
  RealTypeOf: function(v) {
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
