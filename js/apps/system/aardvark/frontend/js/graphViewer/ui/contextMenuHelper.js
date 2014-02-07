/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global document*/
/*global EdgeShaper, modalDialogHelper, uiComponentsHelper*/
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
function ContextMenu(id) {
  "use strict";

  if (id === undefined) {
    throw("An id has to be given.");
  }
  var div,
      ul,
      jqId = "#" + id,
      menu,

    addEntry = function(label, callback) {
      var item, inner;
      item = document.createElement("div");
      item.className = "context-menu-item";
      inner = document.createElement("div");
      inner.className = "context-menu-item-inner";
      inner.appendChild(document.createTextNode(label));
      inner.onclick = function() {
        callback(d3.select(menu.target).data()[0]);
      };
      item.appendChild(inner);
      div.appendChild(item);
    },

    bindMenu = function($objects) {
      menu = $.contextMenu.create(jqId, {
        shadow: false
      });
      $objects.each(function() {
        $(this).bind('contextmenu', function(e){
          menu.show(this,e);
          return false;
        });
      });
    },
    
    divFactory = function() {
      div = document.getElementById(id);
      if (div) {
        div.parentElement.removeChild(div);
      }
      div = document.createElement("div");
      div.className = "context-menu context-menu-theme-osx";
      div.id = id;
      document.body.appendChild(div);
      return div;
    };

  divFactory();

  this.addEntry = addEntry;
  this.bindMenu = bindMenu;
}

