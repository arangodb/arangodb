/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var modalDialogHelper = modalDialogHelper || {};

(function dialogHelper() {
  "use strict";
  
  var 
    bindSubmit = function(button) {
      $(document).bind("keypress.key13", function(e) {
        if (e.which && e.which === 13) {
          $(button).click();
        }
      });
    },
  
    unbindSubmit = function() {
      $(document).unbind("keypress.key13");
    },
  
    createDialogWithObject = function (title, buttonTitle, idprefix, object, callback) {
      var tableToJSON,
        callbackCapsule = function() {
          callback(tableToJSON);
        },
        table = modalDialogHelper.modalDivTemplate(title, buttonTitle, idprefix, callbackCapsule),
        firstRow = document.createElement("tr"),
        firstCell = document.createElement("th"),
        secondCell = document.createElement("th"),
        thirdCell = document.createElement("th"),
        addRow = document.createElement("button"),
        newCounter = 1,
        insertRow;

      tableToJSON = function() {
        var result = {};
        _.each($("#" + idprefix + "table tr:not(#first_row)"), function(tr) {
        
          var key = $(".keyCell input", tr).val(),
            value = $(".valueCell input", tr).val();
          result[key] = value;
        });
        return result;
      };
    
      table.appendChild(firstRow);
      firstRow.id = "first_row";
      firstRow.appendChild(firstCell);
      firstCell.className = "keyCell";
    
      firstRow.appendChild(secondCell);
      secondCell.className = "valueCell";
    
      firstRow.appendChild(thirdCell);
    
      thirdCell.className = "actionCell";
      thirdCell.appendChild(addRow);
    
    
      addRow.id = idprefix + "new";
      addRow.className = "graphViewer-icon-button gv-icon-small add";
    
      insertRow = function(value, key) {
        var internalRegex = /^_(id|rev|key|from|to)/,
          tr = document.createElement("tr"),
          actTh = document.createElement("th"),
          keyTh = document.createElement("th"),
          valueTh = document.createElement("th"),
          deleteInput,
          keyInput,
          valueInput;
        if (internalRegex.test(key)) {
          return;
        }
        table.appendChild(tr);
      
        tr.appendChild(keyTh);
        keyTh.className = "keyCell";
        keyInput = document.createElement("input");
        keyInput.type = "text";
        keyInput.id = idprefix + key + "_key";
        keyInput.value = key;
        keyTh.appendChild(keyInput);
      
      
        tr.appendChild(valueTh);
        valueTh.className = "valueCell";
        valueInput = document.createElement("input");
        valueInput.type = "text";
        valueInput.id = idprefix + key + "_value";
        if ("object" === typeof value) {
          valueInput.value = JSON.stringify(value);
        } else {
          valueInput.value = value;
        }
      
        valueTh.appendChild(valueInput);
      

        tr.appendChild(actTh);
        actTh.className = "actionCell";
        deleteInput = document.createElement("button");
        deleteInput.id = idprefix + key + "_delete";
        deleteInput.className = "graphViewer-icon-button gv-icon-small delete";
      
        actTh.appendChild(deleteInput);
      
        deleteInput.onclick = function() {
          table.removeChild(tr);
        };
      
      };
    
      addRow.onclick = function() {
        insertRow("", "new_" + newCounter);
        newCounter++;
      };
    
      _.each(object, insertRow);
      $("#" + idprefix + "modal").modal('show');
    },
  
    createViewWithObject = function (title, buttonTitle, idprefix, object, callback) {
      var table = modalDialogHelper.modalDivTemplate(title, buttonTitle, idprefix, callback),
        firstRow = document.createElement("tr"),
        firstCell = document.createElement("th"),
        pre = document.createElement("pre");
      table.appendChild(firstRow);
      firstRow.appendChild(firstCell);
      firstCell.appendChild(pre);
      pre.className = "gv_object_view";
      pre.innerHTML = JSON.stringify(object, null, 2);
      $("#" + idprefix + "modal").modal('show');
    },
    
    createTextInput = function(id, value) {
      var input = document.createElement("input");
        input.type = "text";
        input.id = id;
        input.value = value;
        return input;
    },
    
    createCheckboxInput = function(id, selected) {
      var input = document.createElement("input");
      input.type = "checkbox";
      input.id = id;
      input.checked = selected;
      return input;
    },
    
    createListInput = function(id, list, selected) {
      var input = document.createElement("select");
      input.id = id;
      _.each(
        _.sortBy(list, function(e) {
          return e.toLowerCase();
        }), function(entry) {
        var option = document.createElement("option");
        option.value = entry;
        option.selected = (entry === selected);
        option.appendChild(
          document.createTextNode(entry)
        );
        input.appendChild(option);
      });
      return input;
    },
    
    displayDecissionRowsOfGroup = function(group) {
      var rows = $(".decission_" + group),
      selected = $("input[type='radio'][name='" + group + "']:checked").attr("id");
      rows.each(function() {
        if ($(this).attr("decider") === selected) {
          $(this).css("display", "");
        } else {
          $(this).css("display", "none");
        }
      });
    },
    
    insertModalRow,
    
    insertDecissionInput = function(idPre, idPost, group,
      text, isDefault, interior, contentTh, table) {
      var input = document.createElement("input"),
        id = idPre + idPost,
        lbl = document.createElement("label");
      input.id = id;
      input.type = "radio";
      input.name = group;
      input.className = "gv_radio_button";
      lbl.className = "radio";
      contentTh.appendChild(lbl);
      lbl.appendChild(input);
      lbl.appendChild(document.createTextNode(text));
      _.each(interior, function(o) {
        var row = $(insertModalRow(table, idPre, o));
        row.toggleClass("decission_" + group, true);
        row.attr("decider", id);
      });
      if (isDefault) {
        input.checked = true;
      } else {
        input.checked = false;
      }
      lbl.onclick = function(e) {
        displayDecissionRowsOfGroup(group);
        e.stopPropagation();
      };
      displayDecissionRowsOfGroup(group);
    },
    
    insertExtendableInput = function(idPre, idPost, list, contentTh, table, tr) {
      var rows = [],
        i,
        id = idPre + idPost,
        lastId = 1,
        addLineButton = document.createElement("button"),
        input = document.createElement("input"),
        addNewLine = function(content) {
          lastId++;
          var innerTr = document.createElement("tr"),
            innerLabelTh = document.createElement("th"),
            innerContentTh = document.createElement("th"),
            innerInput = document.createElement("input"),
            removeRow = document.createElement("button"),
            lastItem;
          innerInput.type = "text";
          innerInput.id = id + "_" + lastId;
          innerInput.value = content || "";
          if (rows.length === 0) {
            lastItem = $(tr);
          } else {
            lastItem = $(rows[rows.length - 1]);
          }
          lastItem.after(innerTr);
          innerTr.appendChild(innerLabelTh);
          innerLabelTh.className = "collectionTh capitalize";
          innerLabelTh.appendChild(document.createTextNode(idPost + " " + lastId + ":"));
          innerTr.appendChild(innerContentTh);
          innerContentTh.className = "collectionTh";
          innerContentTh.appendChild(innerInput);
          removeRow.id = id + "_" + lastId + "_remove";
          removeRow.className = "graphViewer-icon-button gv-icon-small delete";
          removeRow.onclick = function() {
            table.removeChild(innerTr);
            rows.splice(rows.indexOf(innerTr), 1 );
          };
          
          innerContentTh.appendChild(removeRow);
          rows.push(innerTr);
        };
      input.type = "text";
      input.id = id + "_1";
      contentTh.appendChild(input);
      contentTh.appendChild(addLineButton);
      addLineButton.onclick = function() {
        addNewLine();
      };
      addLineButton.id = id + "_addLine";
      addLineButton.className = "graphViewer-icon-button gv-icon-small add";
      if (list.length > 0) {
        input.value = list[0];
      }
      for (i = 1; i < list.length; i++) {
        addNewLine(list[i]);
      }
    };
    
  insertModalRow = function(table, idprefix, o) {
    var tr = document.createElement("tr"),
      labelTh = document.createElement("th"),
      contentTh = document.createElement("th");
    table.appendChild(tr);
    tr.appendChild(labelTh);
    labelTh.className = "collectionTh";
    if (o.text) {
      labelTh.appendChild(document.createTextNode(o.text + ":"));
    } else {
      labelTh.className += " capitalize";
      if (o.type && o.type === "extenadable") {
        labelTh.appendChild(document.createTextNode(o.id + ":"));
      } else {
        labelTh.appendChild(document.createTextNode(o.id + ":"));
      }
    }
    tr.appendChild(contentTh);
    contentTh.className = "collectionTh";
    switch(o.type) {
      case "text":
        contentTh.appendChild(createTextInput(idprefix + o.id, o.value || ""));
        break;
      case "checkbox":
        contentTh.appendChild(createCheckboxInput(idprefix + o.id, o.selected || false));
        break;
      case "list":
        contentTh.appendChild(createListInput(idprefix + o.id, o.objects, o.selected || undefined));
        break;
      case "extendable":
        insertExtendableInput(idprefix, o.id, o.objects, contentTh, table, tr);
        break;
      case "decission":
        insertDecissionInput(idprefix, o.id, o.group, o.text,
          o.isDefault, o.interior, contentTh, table);
        labelTh.innerHTML = "";
        break;
      default:
        //Sorry unknown
        table.removeChild(tr);
        break;
    }
    return tr;
  };
  
  modalDialogHelper.modalDivTemplate = function (title, buttonTitle, idprefix, callback) {
    // Create needed Elements
    
    buttonTitle = buttonTitle || "Switch";
    
    var div = document.createElement("div"),
    headerDiv = document.createElement("div"),
    buttonDismiss = document.createElement("button"),
    header = document.createElement("a"),
    footerDiv = document.createElement("div"),
    buttonCancel = document.createElement("button"),
    buttonSubmit = document.createElement("button"),
    bodyDiv = document.createElement("div"),
    bodyTable = document.createElement("table");
        
    // Set Classnames and attributes.
    div.id = idprefix + "modal";
    div.className = "modal hide fade";
    div.setAttribute("tabindex", "-1");
    div.setAttribute("role", "dialog");
    div.setAttribute("aria-labelledby", "myModalLabel");
    div.setAttribute("aria-hidden", true);
    div.style.display = "none";
    div.onhidden = function() {
      unbindSubmit();
      document.body.removeChild(div);
    };
      
    headerDiv.className = "modal-header";
    header.className = "arangoHeader";
    buttonDismiss.id = idprefix + "modal_dismiss";
    buttonDismiss.className = "close";
    buttonDismiss.dataDismiss = "modal";
    buttonDismiss.ariaHidden = "true";
    buttonDismiss.appendChild(document.createTextNode("x"));
    
    header.appendChild(document.createTextNode(title));
    
    bodyDiv.className = "modal-body";
    
    bodyTable.id = idprefix + "table";
    
    footerDiv.className = "modal-footer";

    buttonCancel.id = idprefix + "cancel";
    buttonCancel.className = "btn btn-close btn-margin";
    buttonCancel.appendChild(document.createTextNode("Close"));
    
    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "btn btn-success";
    buttonSubmit.style.marginRight = "8px";
    buttonSubmit.appendChild(document.createTextNode(buttonTitle));
    
    // Append in correct ordering
    div.appendChild(headerDiv);
    div.appendChild(bodyDiv);
    div.appendChild(footerDiv);
    
    headerDiv.appendChild(buttonDismiss);
    headerDiv.appendChild(header);
    
    bodyDiv.appendChild(bodyTable);
    
    footerDiv.appendChild(buttonSubmit);
    footerDiv.appendChild(buttonCancel);

    document.body.appendChild(div);
    
    // Add click events
    buttonDismiss.onclick = function() {
      unbindSubmit();
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonCancel.onclick = function() {
      unbindSubmit();
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonSubmit.onclick = function() {
      unbindSubmit();
      callback();
      $("#" + idprefix + "modal").modal('hide');
    };
    bindSubmit(buttonSubmit);
    // Return the table which has to be filled somewhere else
    return bodyTable;
  };
  
  modalDialogHelper.createModalDialog = function(title, idprefix, objects, callback) {
    var table =  modalDialogHelper.modalDivTemplate(title, null, idprefix, callback);
    _.each(objects, function(o) {
      insertModalRow(table, idprefix, o);
    });
    $("#" + idprefix + "modal").modal('show');
  };
  
  modalDialogHelper.createModalChangeDialog = function(title, idprefix, objects, callback) {
    var table =  modalDialogHelper.modalDivTemplate(title, "Change", idprefix, callback);
    _.each(objects, function(o) {
      insertModalRow(table, idprefix, o);
    });
    $("#" + idprefix + "modal").modal('show');
  };
  
  modalDialogHelper.createModalEditDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Save", idprefix, object, callback);
  };
  
  modalDialogHelper.createModalCreateDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Create", idprefix, object, callback);
  };
  
  
  modalDialogHelper.createModalViewDialog = function(title, idprefix, object, callback) {
    createViewWithObject(title, "Edit", idprefix, object, callback);
  };
  
}());
