/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
var modalDialogHelper = modalDialogHelper || {};

(function dialogHelper() {
  "use strict";
  
  modalDialogHelper.modalDivTemplate = function (title, idprefix, callback) {
    // Create needed Elements
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
      document.body.removeChild(div);
    };
      
    headerDiv.className = "modal_header";
    
    buttonDismiss.className = "close";
    buttonDismiss.dataDismiss = "modal";
    buttonDismiss.ariaHidden = "true";
    buttonDismiss.appendChild(document.createTextNode("x"));
    
    header.appendChild(document.createTextNode(title));
    
    bodyDiv.className = "modal_body";
    
    footerDiv.className = "modal_footer";
    
    buttonCancel.id = idprefix + "cancel";
    buttonCancel.className = "btn btn-danger pull-left";
    buttonCancel.appendChild(document.createTextNode("Cancel"));
    
    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "btn btn-success pull-right";
    buttonSubmit.appendChild(document.createTextNode("Switch"));
    
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
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonCancel.onclick = function() {
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonSubmit.onclick = function() {
      callback();
      $("#" + idprefix + "modal").modal('hide');
    };
    
    // Return the table which has to be filled somewhere else
    return bodyTable;
  };
  
  modalDialogHelper.createModalDialog = function(title, idprefix, objects, callback) {
    var table =  modalDialogHelper.modalDivTemplate(title, idprefix, callback);
    
    _.each(objects, function(o) {
      var tr = document.createElement("tr"),
      labelTh = document.createElement("th"),
      contentTh = document.createElement("th"),
      input;
      
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
        default:
          //Sorry unknown
          table.removeChild(tr);
          break;
      }
    });
    $("#" + idprefix + "modal").modal('show');
  };
  
}());