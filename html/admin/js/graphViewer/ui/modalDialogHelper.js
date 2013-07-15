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
      addImg = document.createElement("img"),
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
    addRow.className = "graphViewer-icon-button";
    
    addRow.appendChild(addImg);
    addImg.className = "plusIcon";
    addImg.src = "img/plus_icon.png";
    addImg.width = "16";
    addImg.height = "16";
    
    insertRow = function(value, key) {
      var internalRegex = /^_(id|rev|key|from|to)/,
        tr = document.createElement("tr"),
        actTh = document.createElement("th"),
        keyTh = document.createElement("th"),
        valueTh = document.createElement("th"),
        deleteInput,
        keyInput,
        valueInput,
        delImg;
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
      deleteInput.className = "graphViewer-icon-button";
      
      actTh.appendChild(deleteInput);
      
      delImg = document.createElement("img");
      delImg.src = "img/icon_delete.png";
      delImg.width = "16";
      delImg.height = "16";
      
      deleteInput.appendChild(delImg);
      
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
  };
  
  
  modalDialogHelper.modalDivTemplate = function (title, buttonTitle, idprefix, callback) {
    // Create needed Elements
    
    buttonTitle = buttonTitle || "Switch";
    
    var div = document.createElement("div"),
    headerDiv = document.createElement("div"),
    buttonDismiss = document.createElement("button"),
    header = document.createElement("h3"),
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
    buttonCancel.className = "btn btn-danger pull-left";
    buttonCancel.appendChild(document.createTextNode("Cancel"));
    
    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "btn btn-success pull-right";
    buttonSubmit.appendChild(document.createTextNode(buttonTitle));
    
    // Append in correct ordering
    div.appendChild(headerDiv);
    div.appendChild(bodyDiv);
    div.appendChild(footerDiv);
    
    headerDiv.appendChild(buttonDismiss);
    headerDiv.appendChild(header);
    
    bodyDiv.appendChild(bodyTable);
    
    footerDiv.appendChild(buttonCancel);
    footerDiv.appendChild(buttonSubmit);
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
      var tr = document.createElement("tr"),
        labelTh = document.createElement("th"),
        contentTh = document.createElement("th"),
        input,
        addLineButton,
        rows,
        lastId,
        addNewLine = function() {
          lastId++;
          var innerTr = document.createElement("tr"),
            innerLabelTh = document.createElement("th"),
            innerContentTh = document.createElement("th"),
            innerInput = document.createElement("input"),
            removeRow = document.createElement("button"),
            lastItem;
          innerInput.type = "text";
          innerInput.id = idprefix + o.id + "_" + lastId;
          if (rows.length === 0) {
            lastItem = $(tr);
          } else {
            lastItem = $(rows[rows.length - 1]);
          }
          lastItem.after(innerTr);
          innerTr.appendChild(innerLabelTh);
          innerLabelTh.className = "collectionTh capitalize";
          innerLabelTh.appendChild(document.createTextNode(o.id + " " + lastId + ":"));
          innerTr.appendChild(innerContentTh);
          innerContentTh.className = "collectionTh";
          innerContentTh.appendChild(innerInput);
          removeRow.id = idprefix + o.id + "_" + lastId + "_remove";
          removeRow.onclick = function() {
            table.removeChild(innerTr);
            rows.splice(rows.indexOf(innerTr), 1 );
          };
          
          innerContentTh.appendChild(removeRow);
          rows.push(innerTr);
        };
      
      table.appendChild(tr);
      tr.appendChild(labelTh);
      labelTh.className = "collectionTh capitalize";
      labelTh.appendChild(document.createTextNode(o.id + ":"));
      tr.appendChild(contentTh);
      contentTh.className = "collectionTh";
      switch(o.type) {
        case "text":
          input = document.createElement("input");
          input.type = "text";
          input.id = idprefix + o.id;
          contentTh.appendChild(input);
          break;
        case "checkbox":
          input = document.createElement("input");
          input.type = "checkbox";
          input.id = idprefix + o.id;
          contentTh.appendChild(input);
          break;
        case "list":
          input = document.createElement("select");
          input.id = idprefix + o.id;
          contentTh.appendChild(input);
          _.each(o.objects, function(entry) {
            var option = document.createElement("option");
            option.value = entry;
            option.appendChild(
              document.createTextNode(entry)
            );
            input.appendChild(option);
          });
          break;
        case "extendable":
          rows = [];
          lastId = 1;
          addLineButton = document.createElement("button");
          input = document.createElement("input");
          input.type = "text";
          input.id = idprefix + o.id + "_1";
          contentTh.appendChild(input);
          contentTh.appendChild(addLineButton);
          addLineButton.onclick = addNewLine;
          addLineButton.id = idprefix + o.id + "_addLine";
          break;
        default:
          //Sorry unknown
          table.removeChild(tr);
          break;
      }
    });
    $("#" + idprefix + "modal").modal('show');
  };
  
  modalDialogHelper.createModalEditDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Edit", idprefix, object, callback);
  };
  
  modalDialogHelper.createModalCreateDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Create", idprefix, object, callback);
  };
  
}());