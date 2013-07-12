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

var uiComponentsHelper = uiComponentsHelper || {};

(function componentsHelper() {
  "use strict";
  
  uiComponentsHelper.createButton = function(baseclass, list, title, prefix, callback) {
    var button = document.createElement("li"),
      a = document.createElement("a"),
      label = document.createElement("label");
    button.className = baseclass + "_control " + prefix;
    button.id = prefix;
    button.appendChild(a);
    a.appendChild(label);
    label.appendChild(document.createTextNode(title));
    list.appendChild(button);
    button.onclick = callback;
  };
  
  uiComponentsHelper.createIconButtonBKP = function(icon, prefix, callback) {
    var button = document.createElement("button"),
      i = document.createElement("i");
    button.className = "btn btn-icon";
    button.id = prefix;
    button.appendChild(i);
    i.className = "icon-" + icon + " icon-white";
    button.onclick = callback;
    return button;
  };
  
  uiComponentsHelper.createIconButton = function(icon, prefix, callback) {
    var button = document.createElement("button"),
      i = document.createElement("img");
    button.className = "btn btn-icon";
    button.id = prefix;
    i.className = "gv-icon-btn " + icon;
    button.appendChild(i);
    button.onclick = function() {
      $(".gv-icon-btn").each(function(i, img) {
        $(img).toggleClass("active", false);
      });
      $(i).toggleClass("active", true);
      callback();
    };
    return button;
  };
  
}());