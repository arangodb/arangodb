/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, $, _ */
var uiComponentsHelper = uiComponentsHelper || {};

(function componentsHelper() {
  "use strict";
  
  uiComponentsHelper.createButton = function(baseclass, list, title, prefix, callback) {
    var  button = document.createElement("li");
    button.className = baseclass + "_control " + prefix;
    button.id = prefix;
    button.appendChild(document.createTextNode(title));
    list.appendChild(button);
    button.onclick = callback;
  };
  
}());