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
      var li, button;
      li = document.createElement("li");
      button = document.createElement("button");
      button.onclick = function() {
        callback(d3.select(menu.target).data()[0]);
      };
      button.appendChild(document.createTextNode(label));
      li.appendChild(button);
      ul.appendChild(li);
    },

    bindMenu = function($objects) {
      menu = $.contextMenu.create(jqId);
      $objects.each(function() {
        $(this).bind('contextmenu', function(e){
          menu.show(this,e);
          return false;
        });
      });
    },
    
    divFactory = function() {
      div = document.getElementById(id);
      if (!div) {
        div = document.createElement("div");
        div.id = id;
        ul = document.createElement("ul");
        document.body.appendChild(div);
        div.appendChild(ul);
      }
      ul = div.firstChild;
      return div;
    };

  divFactory();

  this.addEntry = addEntry;
  this.bindMenu = bindMenu;
}

